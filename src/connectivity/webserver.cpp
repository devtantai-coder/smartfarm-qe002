#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../settings.h"
#include "../actuators.h"
#include "../sensors.h"
#include "../control.h"
#include "../demo.h"
#include "../softstart.h"
#include "../wifi.h"
#include "history.h"
#include "weather.h"
#include "../webserver.h"
#include <stdlib.h>

// ── Tạm dừng automation sau lệnh của app ACEBOTT ──
// App QE002 hỏi /all mỗi giây và điều khiển trực tiếp; nếu automation vẫn
// chạy, bơm/đèn có thể bị bật lại ngay sau lệnh app. Tạm dừng ngắn giống
// cơ chế cancelAutoMode của firmware gốc.
static bool sAppPauseActive = false;
static unsigned long sAppPauseUntil = 0;

bool appControlActive() {
  if (!sAppPauseActive) return false;
  if ((int32_t)(millis() - sAppPauseUntil) >= 0) {
    sAppPauseActive = false;
    return false;
  }
  return true;
}

static void pauseAutoForApp() {
  sAppPauseUntil = millis() + APP_CONTROL_PAUSE_MS;
  sAppPauseActive = true;
}

#ifdef SMARTFARM_TEST_HOOKS
void testResetWebStatics() {
  sAppPauseActive = false;
  sAppPauseUntil = 0;
}
#endif

// ── Trang chủ (lưu trong flash, không copy heap mỗi request) ──
static const char ROOT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">\n<meta name="theme-color" content="#2e7d32">
<title>Smart Farm IoT</title>
<!-- Font Awesome tải KHÔNG chặn hiển thị: thiết bị thường chỉ có mạng nội
     bộ (không có Internet) — nếu để link chặn render, trình duyệt phải chờ
     hết thời gian chờ DNS rồi mới hiện trang. media="print" + onload làm
     trang hiện ngay, biểu tượng sẽ tự thêm sau nếu CDN truy cập được. -->
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css" referrerpolicy="no-referrer" media="print" onload="this.media='all'">
<noscript><link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css" referrerpolicy="no-referrer"></noscript>
<style>
:root{--primary:#2e7d32;--primary-dark:#1b5e20;--accent:#1565c0;--danger:#c62828;--bg:#f8fafc;--card-bg:#fff;--text:#1f2937;--muted:#6b7280;--border:#e5e7eb;--radius:14px;--shadow:0 4px 6px -1px rgba(0,0,0,.08),0 2px 4px -1px rgba(0,0,0,.04)}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);line-height:1.6;padding:24px}
.container{max-width:900px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin-bottom:32px;padding-bottom:20px;border-bottom:1px solid var(--border)}
header i{font-size:32px;color:var(--primary)}
header h1{font-size:24px;font-weight:700;letter-spacing:-.5px}
header h1 span{color:var(--primary);font-weight:800}
.ip-badge{margin-left:auto;background:var(--primary);color:#fff;font-size:12px;padding:6px 12px;border-radius:20px;font-weight:700}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;margin-bottom:28px}
.card{background:var(--card-bg);border-radius:var(--radius);padding:20px;box-shadow:var(--shadow);border:1px solid var(--border)}
.card h2{font-size:16px;font-weight:700;margin-bottom:16px;display:flex;align-items:center;gap:8px}
.card h2 i{color:var(--accent)}
.sensor-row{display:flex;align-items:center;justify-content:space-between;padding:12px 0;border-bottom:1px solid var(--border)}
.sensor-row:last-child{border-bottom:none}
.sensor-label{font-size:14px;color:var(--muted);display:flex;align-items:center;gap:8px}
.sensor-value{font-size:16px;font-weight:700;font-variant-numeric:tabular-nums}
.sensor-unit{font-size:13px;color:var(--muted);margin-left:4px}
.badge{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:700;padding:4px 10px;border-radius:999px}
.badge.on{background:#dcfce7;color:#166534}
.badge.off{background:#f1f5f9;color:#64748b}
.controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px;margin-bottom:28px}
.control-group{background:var(--card-bg);border-radius:var(--radius);padding:20px;box-shadow:var(--shadow);border:1px solid var(--border)}
.control-group h3{font-size:15px;font-weight:700;margin-bottom:14px;display:flex;align-items:center;gap:8px}
.control-group h3 i{color:var(--accent)}
.btn-row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}
.btn{flex:1;min-width:70px;padding:10px 14px;border:none;border-radius:10px;font-size:13px;font-weight:600;cursor:pointer;transition:all .15s;display:flex;align-items:center;justify-content:center;gap:6px}
.btn:active{transform:scale(.96)}
.btn:disabled{opacity:.5;cursor:not-allowed}
.btn.primary{background:var(--primary);color:#fff}
.btn.primary:hover{background:var(--primary-dark)}
.btn.secondary{background:var(--accent);color:#fff}
.btn.secondary:hover{background:#0d4f9e}
.btn.danger{background:var(--danger);color:#fff}
.btn.danger:hover{background:#9b2226}
.btn.outline{background:#fff;color:var(--text);border:1px solid var(--border)}
.btn.outline:hover{background:var(--bg)}
.slider-group{margin-top:14px}
.slider-group label{font-size:13px;color:var(--muted);display:block;margin-bottom:6px;display:flex;justify-content:space-between}
.slider-group input[type=range]{width:100%;accent-color:var(--primary)}
.status-bar{display:flex;gap:12px;margin-top:20px;padding-top:16px;border-top:1px solid var(--border)}
.status-item{flex:1;font-size:13px;color:var(--muted);display:flex;align-items:center;gap:6px}
.refresh{font-size:11px;color:var(--muted);text-align:center;margin-top:10px}
@media(max-width:600px){body{padding:16px}header{flex-direction:column;align-items:flex-start;gap:8px}.ip-badge{margin-left:0;margin-top:6px}}
/* --- responsive & motion polish --- */
@media (max-width:640px){body{padding:12px} .grid{grid-template-columns:1fr;gap:12px} .controls{grid-template-columns:1fr;gap:12px} .btn{min-height:44px;font-size:14px} header h1{font-size:20px}}
.btn{transition:transform .12s ease,box-shadow .12s ease,background .12s ease}
.btn:active{transform:scale(.97)}
.card{transition:box-shadow .2s ease,transform .2s ease}
@media (hover:hover){.card:hover{box-shadow:0 10px 24px -6px rgba(0,0,0,.12)}}
@media (prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}
.ip-badge{backdrop-filter:blur(6px)}
.chart-wrap{position:relative;width:100%;overflow:hidden;border:1px solid var(--border);border-radius:10px;background:#fff}
#historyChart{display:block;width:100%;height:240px}
.chart-legend{display:flex;gap:14px;flex-wrap:wrap;font-size:12px;color:var(--muted);margin-top:10px}
.chart-legend i{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:5px;vertical-align:middle}
.outdoor{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px;margin-bottom:14px}
.outdoor .item{background:var(--bg);border-radius:10px;padding:10px 12px}
.outdoor .label{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.04em}
.outdoor .value{font-size:18px;font-weight:700;margin-top:2px}
</style>
</head>
<body>
<div class="container">
<header>
<i class="fa-solid fa-leaf"></i>
<h1>Smart Farm <span>IoT</span></h1>
<span class="ip-badge" id="ipBadge">--.--.--.--</span>
</header>

<section class="card">
<h2><i class="fa-solid fa-chart-mixed"></i> Cảm biến</h2>
<div id="sensorData">
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-temperature-half"></i> Nhiệt độ</div><div class="sensor-value" id="valTemp">--</div><div class="sensor-unit">&deg;C</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-water"></i> Độ ẩm KK</div><div class="sensor-value" id="valHum">--</div><div class="sensor-unit">%</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-sun"></i> Ánh sáng</div><div class="sensor-value" id="valLight">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-mound"></i> Độ ẩm đất</div><div class="sensor-value" id="valSoil">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-tint"></i> Mực nước</div><div class="sensor-value" id="valWater">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-cloud-sun"></i> Thời tiết</div><div class="sensor-value" id="valWeather">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-person-walking"></i> Chuyển động</div><div class="sensor-value" id="valPir">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-hand-point-up"></i> Cảm ứng</div><div class="sensor-value" id="valTouch">--</div></div>\n</div>
</section>

<section class="card">
<h2><i class="fa-solid fa-chart-line"></i> Thời tiết Internet &amp; lịch sử</h2>
<div class="outdoor">
<div class="item"><div class="label">Ngoài trời</div><div class="value" id="outTemp">--</div></div>
<div class="item"><div class="label">Độ ẩm ngoài trời</div><div class="value" id="outHum">--</div></div>
<div class="item"><div class="label">Thời tiết</div><div class="value" id="outDesc" style="font-size:15px">--</div></div>
<div class="item"><div class="label">Cập nhật</div><div class="value" id="outAge" style="font-size:15px">--</div></div>
</div>
<div class="chart-wrap"><canvas id="historyChart"></canvas></div>
<div class="chart-legend">
<span><i style="background:#e53935"></i>Nhiệt độ</span>
<span><i style="background:#1e88e5"></i>Độ ẩm KK</span>
<span><i style="background:#43a047"></i>Độ ẩm đất</span>
<span><i style="background:#00acc1"></i>Mực nước</span>
<span><i style="background:#fb8c00"></i>Ánh sáng</span>
</div>
<div class="refresh" id="chartInfo">Lịch sử lưu tạm trong RAM, tự xóa khi khởi động lại.</div>
</section>

<section class="card">
<h2><i class="fa-solid fa-sliders"></i> Điều khiển</h2>
<div class="controls">
<div class="control-group">
<h3><i class="fa-solid fa-water"></i> Máy bơm</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('pump')"><i class="fa-solid fa-power-off"></i> Bật/Tắt</button>
<button class="btn secondary" onclick="setAutoMode('pump',true)">Tự động</button>
<button class="btn outline" onclick="setAutoMode('pump',false)">Thủ công</button>
</div>
<div class="badge off" id="badgePump"><i class="fa-solid fa-circle"></i> Tắt</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-lightbulb"></i> Đèn xanh</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('led')"><i class="fa-solid fa-power-off"></i> Bật/Tắt</button>
<button class="btn secondary" onclick="ledSet(true)"><i class="fa-solid fa-play"></i> Bật</button>
<button class="btn outline" onclick="ledSet(false)"><i class="fa-solid fa-stop"></i> Tắt</button>
</div>
<div class="badge off" id="badgeLed"><i class="fa-solid fa-circle"></i> Tắt</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-fan"></i> Quạt</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('fan')"><i class="fa-solid fa-power-off"></i> Bật/Tắt</button>
<button class="btn secondary" onclick="setAutoMode('fan',true)">Tự động</button>
<button class="btn outline" onclick="setAutoMode('fan',false)">Thủ công</button>
</div>
<div class="badge off" id="badgeFan"><i class="fa-solid fa-circle"></i> Tắt</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-palette"></i> RGB</h3>
<div class="btn-row">
<button class="btn danger" onclick="setRgb(255,0,0)"><i class="fa-solid fa-circle"></i> Đỏ</button>
<button class="btn primary" onclick="setRgb(0,255,0)"><i class="fa-solid fa-circle"></i> Xanh lá</button>
<button class="btn secondary" onclick="setRgb(0,0,255)"><i class="fa-solid fa-circle"></i> Xanh dương</button>
<button class="btn outline" onclick="setRgb(255,255,255)"><i class="fa-solid fa-circle"></i> Trắng</button>
<button class="btn danger" onclick="setRgb(0,0,0)"><i class="fa-solid fa-circle-stop"></i> Tắt</button>
</div>
<div class="badge on" id="badgeRgbAuto"><i class="fa-solid fa-wand-magic-sparkles"></i> Tự động: Bật</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-heart-pulse"></i> Đèn thở</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('breathe')"><i class="fa-solid fa-power-off"></i> Bật/Tắt</button>
<button class="btn secondary" onclick="setAutoMode('breathe',true)">Tự động</button>
<button class="btn outline" onclick="setAutoMode('breathe',false)">Thủ công</button>
</div>
<div class="badge off" id="badgeBreathe"><i class="fa-solid fa-circle"></i> Tắt</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-bell"></i> Báo động</h3>
<div class="btn-row">
<button class="btn danger" onclick="toggleDevice('alarm')"><i class="fa-solid fa-bell"></i> Bật/Tắt còi</button>
</div>
<div class="badge off" id="badgeAlarm"><i class="fa-solid fa-circle"></i> Tắt</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-circle-arrow-right"></i> Servo (cửa)</h3>
<div class="slider-group">
<label><span id="servoLabel">0</span> độ</label>
<input type="range" id="servoSlider" min="0" max="180" value="0" onchange="setServo(this.value)">
</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-wheat-awn"></i> Cối xay gió &amp; Cho ăn</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleWindmill()"><i class="fa-solid fa-rotate"></i> Cối xay gió</button>
<button class="btn secondary" onclick="feedNow()"><i class="fa-solid fa-bowl-food"></i> Cho ăn</button>
</div>
<div class="badge off" id="badgeWindmill"><i class="fa-solid fa-circle"></i> Tắt</div>
</div>
</div>
<div class="control-group" style="grid-column:1/-1">
<h3><i class="fa-solid fa-robot"></i> Chế độ tự động</h3>
<div class="btn-row">
<button class="btn outline" onclick="setAutoMode('pump',true)"><i class="fa-solid fa-tint"></i> Bơm TĐ</button>
<button class="btn outline" onclick="setAutoMode('pump',false)"><i class="fa-solid fa-hand"></i> Bơm TC</button>
<button class="btn outline" onclick="setAutoMode('light',true)"><i class="fa-solid fa-lightbulb"></i> Đèn TĐ</button>
<button class="btn outline" onclick="setAutoMode('light',false)"><i class="fa-solid fa-hand"></i> Đèn TC</button>
<button class="btn outline" onclick="setAutoMode('rgb',true)"><i class="fa-solid fa-palette"></i> RGB TĐ</button>
<button class="btn outline" onclick="setAutoMode('rgb',false)"><i class="fa-solid fa-hand"></i> RGB TC</button>
<button class="btn outline" onclick="setAutoMode('fan',true)"><i class="fa-solid fa-fan"></i> Quạt TĐ</button>
<button class="btn outline" onclick="setAutoMode('fan',false)"><i class="fa-solid fa-hand"></i> Quạt TC</button>
<button class="btn outline" onclick="setAutoMode('breathe',true)"><i class="fa-solid fa-heart-pulse"></i> Thở TĐ</button>
<button class="btn outline" onclick="setAutoMode('breathe',false)"><i class="fa-solid fa-hand"></i> Thở TC</button>
<button class="btn danger" onclick="runDemo()"><i class="fa-solid fa-clapperboard"></i> Trình diễn hoạt động</button>
</div>
<div class="status-bar">
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Bơm: <span id="statusPumpAuto">Bật</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Đèn: <span id="statusLightAuto">Bật</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> RGB: <span id="statusRgbAuto">Bật</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Quạt: <span id="statusFanAuto">Bật</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Thở: <span id="statusBreatheAuto">Bật</span></div>
<div class="status-item"><i class="fa-solid fa-clapperboard"></i> Demo: <span id="statusDemo">Tắt</span></div>
</div>
</div>
</section>
<div class="refresh">Cập nhật mỗi 2 giây &bull; Smart Farm QE002 ACEBOTT</div>
</div>
<script>
const params=new URLSearchParams(location.search);
let TOKEN=params.get('token')||'';
try{
if(TOKEN)sessionStorage.setItem('sfToken',TOKEN);
else TOKEN=sessionStorage.getItem('sfToken')||'';
}catch(e){}
if(params.get('token'))history.replaceState(null,'',location.pathname);
let NONCE=0;
// Poll thích ứng: 2 giây khi dữ liệu còn đổi (hoặc người dùng vừa thao tác),
// 5 giây khi bảng điều khiển đứng yên và không có gì thay đổi -> thiết bị nhận
// ít request hơn mà vẫn cập nhật ngay khi có thay đổi.
let pollMs=2000,pollTimer=null,lastJson='',stable=0;
function schedulePoll(){clearTimeout(pollTimer);pollTimer=setTimeout(pollTick,pollMs)}
function wakePoll(){stable=0;pollMs=2000;schedulePoll()}
function authHeaders(){return TOKEN?{'X-Auth-Token':TOKEN}:{}}
// Mọi request có thời gian chờ: nếu thiết bị treo, fetch sẽ treo vô hạn và
// giao diện đứng hình (mất cả vòng poll). Abort sau 5 s thì giao diện tự
// phục hồi ở vòng poll kế tiếp.
async function apiGet(u,ms){
const c=new AbortController();
const t=setTimeout(()=>c.abort(),ms||5000);
try{return await fetch(u,{headers:authHeaders(),signal:c.signal})}
finally{clearTimeout(t)}
}
function api(u){
  let sep=u.indexOf('?')>=0?'&':'?';
  if(NONCE>0)u=u+sep+'nonce='+NONCE;
  return u
}
async function call(u){
let r=await apiGet(api(u));
// 403 = nonce đã cũ (lệnh trước bị mất phản hồi, hoặc thiết bị vừa khởi
// động lại). Lấy nonce mới rồi thử lại một lần thay vì kẹt 403 mãi mãi.
if(NONCE>0&&r.status===403){
  try{const rr=await apiGet('/api');const dd=await rr.json();NONCE=dd.nonce??0}catch(e){}
  r=await apiGet(api(u));
}
if(NONCE>0&&(r.ok||r.status===400))NONCE++;
if(r.ok)wakePoll();
return r}
let polling=false;
async function fetchData(){
if(polling)return false;polling=true;
try{
const r=await apiGet('/api');
if(r.status===401){document.getElementById('ipBadge').textContent='Can token';return false}
const d=await r.json();
window.__sf=d;
NONCE=d.nonce??0;
document.getElementById('valTemp').textContent=d.temp?.toFixed(1)??'--';
document.getElementById('valHum').textContent=d.hum?.toFixed(1)??'--';
document.getElementById('valLight').textContent=d.light??'--';
document.getElementById('valSoil').textContent=d.soil??'--';
document.getElementById('valWater').textContent=d.water??'--';
const wt=['Bình thường','Nắng','Âm u','Mưa'];
document.getElementById('valWeather').textContent=wt[d.weather]??'--';
document.getElementById('valPir').textContent=d.pir?'Có':'Không';
document.getElementById('valTouch').textContent=d.touch?'Chạm':'Không';
const p=document.getElementById('badgePump');p.textContent=d.relay?'Bật':'Tắt';p.className='badge '+(d.relay?'on':'off');
const l=document.getElementById('badgeLed');l.textContent=d.led?'Bật':'Tắt';l.className='badge '+(d.led?'on':'off');
const f=document.getElementById('badgeFan');f.textContent=d.fan?'Bật':'Tắt';f.className='badge '+(d.fan?'on':'off');
const b=document.getElementById('badgeBreathe');b.textContent=d.breathe?'Bật':'Tắt';b.className='badge '+(d.breathe?'on':'off');
const a=document.getElementById('badgeAlarm');a.textContent=d.alarm?'Bật':'Tắt';a.className='badge '+(d.alarm?'on':'off');
const w=document.getElementById('badgeWindmill');w.textContent=d.windmill?'Đang quay':(d.feed?'Đang cho ăn':'Tắt');w.className='badge '+(d.windmill||d.feed?'on':'off');
document.getElementById('statusPumpAuto').textContent=d.pumpAuto?'Bật':'Tắt';
document.getElementById('statusLightAuto').textContent=d.lightAuto?'Bật':'Tắt';
document.getElementById('statusRgbAuto').textContent=d.rgbAuto?'Bật':'Tắt';
document.getElementById('statusFanAuto').textContent=d.fanAuto?'Bật':'Tắt';
document.getElementById('statusBreatheAuto').textContent=d.breatheAuto?'Bật':'Tắt';
document.getElementById('statusDemo').textContent=d.demo?('Đang chạy: '+d.demoStep):'Tắt';
document.getElementById('badgeRgbAuto').textContent=d.rgbAuto?'Tự động: Bật':'Tự động: Tắt';
document.getElementById('badgeRgbAuto').className='badge '+(d.rgbAuto?'on':'off');
const j=JSON.stringify(d);
const changed=(j!==lastJson);
lastJson=j;
return changed;
}catch(e){console.error(e);return false}finally{polling=false}
}
async function toggleDevice(id){await call('/toggle?id='+id);fetchData()}
async function ledSet(on){await call(on?'/on':'/off');fetchData()}
async function toggleWindmill(){const s=window.__sf||{};await call('/windmill?on='+(s.windmill?'false':'true'));fetchData()}
async function feedNow(){await call('/feed');fetchData()}
async function runDemo(){if(!confirm('Chạy trình diễn lần lượt các hoạt động (khoảng 40 giây)?'))return;await call('/demo');fetchData()}
async function setAutoMode(id,on){await call('/auto?id='+id+'&on='+on);fetchData()}
async function setRgb(r,g,b){await call('/rgb?r='+r+'&g='+g+'&b='+b);fetchData()}
async function setServo(v){document.getElementById('servoLabel').textContent=v;await call('/servo?angle='+v)}
let lastExtras=0;
function drawHistory(d){
const cv=document.getElementById('historyChart');
if(!cv||!cv.getContext)return;
const ctx=cv.getContext('2d');if(!ctx||!d||!d.samples)return;
const samples=d.samples, W=cv.clientWidth||800, H=240;
const dpr=window.devicePixelRatio||1;
cv.width=Math.round(W*dpr);cv.height=Math.round(H*dpr);
ctx.setTransform(dpr,0,0,dpr,0,0);
ctx.clearRect(0,0,W,H);
const pad={l:34,r:8,t:10,b:18};
const gw=W-pad.l-pad.r, gh=H-pad.t-pad.b;
ctx.strokeStyle='#e5e7eb';ctx.lineWidth=1;
ctx.fillStyle='#6b7280';ctx.font='11px system-ui,sans-serif';
for(let i=0;i<=4;i++){const y=pad.t+gh*i/4;ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(pad.l+gw,y);ctx.stroke();ctx.fillText(String(100-i*25),4,y+4)}
if(samples.length<2){ctx.textAlign='center';ctx.fillText('Chưa đủ dữ liệu lịch sử',pad.l+gw/2,pad.t+gh/2);ctx.textAlign='left';return}
const sx=gw/(samples.length-1);
function line(key,scale,color){
ctx.beginPath();ctx.strokeStyle=color;ctx.lineWidth=2;
for(let i=0;i<samples.length;i++){const v=Math.max(0,Math.min(1,(samples[i][key]||0)/scale));const x=pad.l+i*sx,y=pad.t+gh*(1-v);if(i)ctx.lineTo(x,y);else ctx.moveTo(x,y)}
ctx.stroke();
}
line('temp',50,'#e53935');
line('hum',100,'#1e88e5');
line('soil',4095,'#43a047');
line('water',4095,'#00acc1');
line('light',4095,'#fb8c00');
}
async function refreshExtras(){
const cv=document.getElementById('historyChart');
if(!cv||!cv.getContext)return;
try{
const wr=await apiGet('/weather');
if(wr.ok){const w=await wr.json();
document.getElementById('outTemp').textContent=w.valid?w.temp.toFixed(1)+' °C':'--';
document.getElementById('outHum').textContent=w.valid?Math.round(w.hum)+' %':'--';
document.getElementById('outDesc').textContent=w.valid?w.desc:'--';
document.getElementById('outAge').textContent=w.valid?(w.age<60?'Vừa cập nhật':Math.round(w.age/60)+' phút trước'):'Chưa có';
}
const hr=await apiGet('/history');
if(hr.ok){const h=await hr.json();drawHistory(h);
document.getElementById('chartInfo').textContent='Đang hiển thị '+h.count+' mẫu, mỗi '+h.interval+' giây một mẫu (lưu tạm trong RAM).';
}
}catch(e){console.error(e)}
}
async function pollTick(){
if(!document.hidden){const changed=await fetchData();if(changed){stable=0;pollMs=2000}else{stable++;if(stable>=3)pollMs=5000}}
if(Date.now()-lastExtras>60000){lastExtras=Date.now();refreshExtras()}
schedulePoll();
}
document.addEventListener('visibilitychange',()=>{if(!document.hidden){wakePoll();lastExtras=0;fetchData()}});
wakePoll();
fetchData();
</script>
</body>
</html>
)rawliteral";

// ── Bảo vệ web ──
// 1) Chống CSRF: trình duyệt gửi kèm Origin/Referer/Sec-Fetch-Site khi trang
//    khác gọi vào thiết bị; chỉ chấp nhận yêu cầu cùng nguồn với thiết bị.
// 2) Token tùy chọn: nếu WEB_TOKEN được đặt trong config.h, mọi lệnh đổi
//    trạng thái (và cả /api vì trả nonce + dữ liệu cảm biến) phải kèm đúng
//    token qua header X-Auth-Token; ?token= vẫn chấp nhận cho client tự viết.
//    Trang chủ cũng đọc ?token= một lần rồi tự gửi bằng header, không để
//    token dính vào URL các lệnh phía sau.
static bool ciEqual(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char ca = a[i], cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return true;
}

// So tên host (bỏ phần cổng) không phân biệt hoa/thường.
// Cổng bị bỏ qua có chủ đích: thiết bị chỉ phục vụ web trên MỘT cổng, nên
// "cùng tên, khác cổng" vẫn là chính thiết bị này chứ không phải nguồn lạ.
static bool hostNameEq(const char* a, size_t alen, const char* b) {
  if (b == nullptr || b[0] == '\0') return false;
  size_t an = 0;
  while (an < alen && a[an] != ':') an++;
  size_t bn = 0;
  while (b[bn] != '\0' && b[bn] != ':') bn++;
  if (an != bn) return false;
  return ciEqual(a, b, an);
}

// IP hiện tại dạng chuỗi — dùng cache chung với OLED (wifiIpString) để không
// hỏi driver WiFi trên mỗi request được bảo vệ.
static const char* localIpString() { return wifiIpString(); }

// Thiết bị trả lời trên ba "tên" hợp lệ: IP hiện tại, tên host mà chính
// firmware quảng bá qua mDNS (ArduinoOTA.setHostname("smartfarm")), và tên đầy
// đủ "smartfarm.local". Trước đây chỉ so với "http://<localIP>", nên mở
// dashboard bằng http://smartfarm.local làm MỌI lệnh điều khiển trả 403.
//
// mDNS chỉ tồn tại khi ArduinoOTA.begin() được gọi (OTA_PASSWORD khác rỗng);
// khi OTA tắt, firmware không quảng bá tên nào nên allowlist cũng không chứa
// tên đó — Origin "http://smartfarm.local" lúc này là một nguồn không tồn tại.
//
// KHÔNG tin header Host của chính request: kẻ tấn công DNS-rebinding cho tên
// miền của hắn trỏ về IP thiết bị thì Host + Origin đều là tên của hắn và sẽ
// khớp nhau — đúng lỗ hổng mà kiểm tra Origin sinh ra để chặn.
static bool sameOriginCStr(const char* url) {
  if (url == nullptr || url[0] == '\0') return true;
  const char* p = url;
  if (strncmp(p, "http://", 7) == 0) p += 7;
  else if (strncmp(p, "https://", 8) == 0) p += 8;
  else return false;   // "null" (iframe sandbox) hoặc scheme lạ

  size_t hlen = 0;
  while (p[hlen] && p[hlen] != '/' && p[hlen] != '?' && p[hlen] != '#') hlen++;
  if (hlen == 0) return false;

  if (hostNameEq(p, hlen, localIpString())) return true;
  if (sys.otaPassword[0] == '\0') return false;   // không có mDNS/OTA
  if (hostNameEq(p, hlen, "smartfarm")) return true;
  return hostNameEq(p, hlen, "smartfarm.local");
}

// So sánh token kiểu constant-time để không rò rỉ độ dài/ký tự khớp qua thời gian phản hồi.
static bool secretMatches(const char* a, const char* b) {
  size_t la = strlen(a), lb = strlen(b);
  size_t diff = la ^ lb;
  for (size_t i = 0; i < la && i < lb; i++) diff |= (size_t)(a[i] ^ b[i]);
  return diff == 0;
}

// Trình duyệt LUÔN gửi User-Agent. Dùng nó để phân biệt "curl/app nội bộ"
// (không có header nguồn vì không phải trình duyệt) với "trang web cố tình
// giấu nguồn" (referrer-policy: no-referrer, <img referrerpolicy=no-referrer>).
static bool looksLikeBrowser(const String& ua) {
  const char* s = ua.c_str();
  return s != nullptr && strstr(s, "Mozilla") != nullptr;
}

static bool checkSameOrigin() {
  // Đọc mỗi header đúng MỘT lần (WebServer cấp phát String cho mỗi lần đọc);
  // header vắng trả chuỗi rỗng nên không cần hasHeader() gọi thêm.
  const String origin = server.header("Origin");
  const String referer = server.header("Referer");
  const String site = server.header("Sec-Fetch-Site");

  if (origin.length() == 0 && referer.length() == 0 && site.length() == 0) {
    // Không có header nguồn: client không phải trình duyệt (curl, app tự viết).
    // Nhưng nếu UA là của trình duyệt thì request được tạo từ một trang khác
    // đã giấu Referer — đúng kịch bản CSRF — nên phải từ chối.
    if (looksLikeBrowser(server.header("User-Agent"))) {
      server.send(403, "text/plain", "forbidden");
      return false;
    }
    return true;
  }

  if (origin.length() > 0 && !sameOriginCStr(origin.c_str())) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  if (referer.length() > 0 && !sameOriginCStr(referer.c_str())) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  // "same-site" (subdomain/khác cổng cùng site) vẫn là nguồn KHÁC → chặn.
  if (strcmp(site.c_str(), "cross-site") == 0 || strcmp(site.c_str(), "same-site") == 0) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  return true;
}

static bool checkToken() {
  if (sys.webToken[0] == '\0') return true;
  // Đọc mỗi nguồn token ĐÚNG MỘT lần; so sánh trực tiếp con trỏ, tránh
  // copy vào buffer trung gian (tiết kiệm cấp phát + strncpy).
  const String hdr = server.header("X-Auth-Token");
  const String q = (hdr.length() > 0) ? String() : server.arg("token");
  const char* provided = (hdr.length() > 0) ? hdr.c_str()
                      : (q.length() > 0) ? q.c_str() : "";
  if (!secretMatches(provided, sys.webToken)) {
    server.send(401, "text/plain", "unauthorized");
    return false;
  }
  return true;
}

// Bảo vệ đầy đủ cho các lệnh đổi trạng thái: chống CSRF + token + chống replay.
static bool guardWeb() {
  if (!checkSameOrigin()) return false;
  if (!checkToken()) return false;

#if ANTI_REPLAY
  // Chống replay: yêu cầu nonce lớn hơn lần dùng cuối cùng.
  // So sánh bằng hiệu có dấu (int32) để bộ đếm tràn 32 bit vẫn đúng: nếu
  // dùng phép so sánh thường thì một nonce = 0xFFFFFFFF sẽ "chết" mọi lệnh
  // sau đó (không còn số nào lớn hơn được), tức là tự khóa thiết bị.
  const String nonceStr = server.arg("nonce");
  uint32_t nonce = (uint32_t)strtoul(nonceStr.c_str(), nullptr, 10);
  if (nonce == 0 || (int32_t)(nonce - sys.gLastNonce) <= 0) {
    server.send(403, "text/plain", "replay");
    return false;
  }
  sys.gLastNonce = nonce;
#endif

  return true;
}

bool webTokenGuard() { return checkToken(); }
bool webGuard() { return guardWeb(); }

void handleRoot() {
  server.sendHeader("X-Frame-Options", "DENY");   // chống nhúng iframe (clickjacking)
  // Trang tĩnh theo từng bản firmware: ETag cho phép trình duyệt hỏi lại và
  // nhận 304 (không body) thay vì tải lại 16 KB mỗi lần mở dashboard.
  if (server.header("If-None-Match") == WEB_ROOT_ETAG) {
    server.send(304, "text/plain", "");
    return;
  }
  server.sendHeader("ETag", WEB_ROOT_ETAG);
  // "no-cache" = luôn xác thực lại bằng ETag; dữ liệu động đi qua /api.
  server.sendHeader("Cache-Control", "private, no-cache");
  server.send_P(200, "text/html", ROOT_HTML);
}

// ── API: dữ liệu cảm biến JSON (dùng biến global đã lọc, không đọc cảm biến thô) ──
// Dựng JSON của /api vào buffer cho trước.
// Trả về số byte CẦN (không tính byte NUL) — giống snprintf. Nếu kết quả >=
// kích thước buffer thì JSON bị cắt cụt.
static int buildApiJson(char* buf, size_t cap) {
  const bool demoOn = demoRunning();   // đọc một lần, tránh gọi lặp 2 chỗ trong snprintf
  // NaN/Inf không phải số JSON hợp lệ ("nan"/"inf" làm hỏng cả response);
  // quy về 0 — cùng quy ước với floatArg() khi nhận tham số.
  const float temp = isfinite(sys.gTemp) ? sys.gTemp : 0.0f;
  const float hum = isfinite(sys.gHum) ? sys.gHum : 0.0f;
  return snprintf(buf, cap,
           "{\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,"
           "\"weather\":%d,\"pir\":%s,\"touch\":%s,\"relay\":%s,\"fan\":%s,"
           "\"led\":%s,\"breathe\":%s,\"alarm\":%s,\"windmill\":%s,\"feed\":%s,\"demo\":%s,"
           "\"demoStep\":\"%s\",\"pumpAuto\":%s,\"lightAuto\":%s,\"rgbAuto\":%s,"
           "\"fanAuto\":%s,\"breatheAuto\":%s,\"nonce\":%u}",
           (double)temp, (double)hum, (int)sys.gLight, (int)sys.gSoil, (int)sys.gWater,
           (int)weatherStatus(),
           sys.gPir ? "true" : "false", sys.gTouch ? "true" : "false",
           sys.relayState ? "true" : "false", sys.fanState ? "true" : "false",
           sys.blueLedState ? "true" : "false", sys.breatheLedState ? "true" : "false",
           manualAlarm() ? "true" : "false",
           sys.windmillOn ? "true" : "false", feedRunning() ? "true" : "false",
           demoOn ? "true" : "false",
           demoOn ? demoStepName() : "",
           sys.pumpAuto ? "true" : "false", sys.lightAuto ? "true" : "false",
           sys.rgbAuto ? "true" : "false", sys.fanAuto ? "true" : "false",
           sys.breatheAuto ? "true" : "false",
           (unsigned)(sys.gLastNonce == 0xFFFFFFFFul ? 1ul : sys.gLastNonce + 1));  // tránh trả nonce 0 (luôn bị từ chối)
}

#ifdef SMARTFARM_TEST_HOOKS
// Hook cho bộ test: dựng JSON vào buffer bất kỳ để kiểm tra nhánh "bị cắt".
int testBuildApiJson(char* buf, size_t cap) { return buildApiJson(buf, cap); }
#endif

void handleApi() {
  // Khi WEB_TOKEN được đặt, dữ liệu cảm biến + nonce chỉ trả cho client đã xác thực.
  // Ở đây chỉ kiểm tra token (không kèm chống replay) vì client cần /api để LẤY nonce.
  if (!checkToken()) return;

  // Dùng giá trị global đã lọc — không đọc cảm biến thô ở đây
  // (DHT11 cần 1 giây giữa hai lần đọc; đọc thêm sẽ trả dữ liệu cũ)
  // Dựng JSON bằng snprintf vào buffer cố định — không tự nối String trong handler
  // và không cấp phát heap thêm (send() của thư viện vẫn tạo String nội bộ một lần).
  char buf[API_JSON_BUF_SIZE];
  int n = buildApiJson(buf, sizeof(buf));

  // snprintf trả về số byte SẼ cần (không tính byte NUL). Nếu >= sizeof(buf)
  // thì JSON bị cắt cụt -> trình duyệt không parse được. Báo lỗi tường minh
  // thay vì trả về JSON hỏng.
  if (n < 0 || (size_t)n >= sizeof(buf)) {
    server.send(500, "application/json", "{\"error\":\"response too large\"}");
    return;
  }

  // Ghi thẳng buffer qua sendContent: send(code, type, content) của WebServer
  // nhận String nên sẽ copy ~400 byte sang heap mỗi lần /api được gọi (2 giây
  // một lần khi mở trang). setContentLength + sendContent giữ nguyên header
  // Content-Length mà không cấp phát thêm.
  // snprintf đã trả số byte cần (n); dùng lại thay vì strlen() quét lại buffer.
  size_t len = (size_t)n;
  server.setContentLength(len);
  server.send(200, "application/json", "");
  server.sendContent(buf, len);
}

static uint8_t clampByte(long v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

// Đọc tham số float từ query. Đọc ĐÚNG MỘT lần: WebServer cấp phát một
// String mới cho mỗi lần đọc nên gọi hasArg() rồi arg() sẽ cấp phát 3 lần.
// Giá trị không hữu hạn (client gửi "nan"/"inf") được quy về 0 giống
// toFloat("abc") — nếu không, NaN sẽ lọt qua constrain() (mọi so sánh với
// NaN đều sai) rồi gây ép kiểu float->int không xác định trong các phép so
// ngưỡng và làm logic điều khiển kẹt.
// Trả false nếu tham số không có (hoặc rỗng) -> không thay đổi ngưỡng nào.
static bool floatArg(const char* name, float* out) {
  const String v = server.arg(name);
  if (v.length() == 0) return false;
  const float f = v.toFloat();
  *out = isfinite(f) ? f : 0.0f;
  return true;
}

// Bản NGHIÊM TÚC cho các tham số mà "0" là một giá trị có nghĩa (offset hiệu
// chuẩn): String::toFloat() trả 0 cho "abc" nên một phím lỗi khi gõ sẽ âm thầm
// xóa luôn độ lệch đang dùng. Ở đây chuỗi phải parse hết và ra số hữu hạn.
bool webFloatArg(const char* name, float* out) {
  const String v = server.arg(name);
  if (v.length() == 0) return false;
  char* end = nullptr;
  const float f = strtof(v.c_str(), &end);
  if (end == v.c_str()) return false;              // không phải số
  while (*end == ' ' || *end == '\t') end++;       // "12abc"/"1 " -> vẫn từ chối
  if (*end != '\0' || !isfinite(f)) return false;
  *out = f;
  return true;
}

// ── Bật/tắt thiết bị (thao tác thủ công sẽ tắt chế độ tự động tương ứng) ──
void handleToggle() {
  if (!guardWeb()) return;
  const String idArg = server.arg("id");
  const char* id = idArg.c_str();
  if (strcmp(id, "pump") == 0) {
    setPumpAuto(false);
    setRelay(!sys.relayState);
  } else if (strcmp(id, "led") == 0) {
    setLightAuto(false);
    setBlueLed(!sys.blueLedState);
  } else if (strcmp(id, "fan") == 0) {
    setFanAuto(false);
    setFan(!sys.fanState);
  } else if (strcmp(id, "breathe") == 0) {
    bool want = !sys.breatheLedState;
    setBreatheAuto(false);
    setBreatheLed(want);
  } else if (strcmp(id, "alarm") == 0 || strcmp(id, "buzzer") == 0) {
    setManualAlarm(!manualAlarm());
  } else {
    server.send(400, "text/plain", "unknown id");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Chế độ tự động ──
void handleAuto() {
  if (!guardWeb()) return;
  const String idArg = server.arg("id");
  const char* id = idArg.c_str();
  bool on = server.arg("on") == "true";
  if (strcmp(id, "pump") == 0) setPumpAuto(on);
  else if (strcmp(id, "light") == 0) setLightAuto(on);
  else if (strcmp(id, "rgb") == 0) setRgbAuto(on);
  else if (strcmp(id, "fan") == 0) setFanAuto(on);
  else if (strcmp(id, "breathe") == 0) {
    setBreatheAuto(on);
    setBreatheLed(on);
  } else {
    server.send(400, "text/plain", "unknown id");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Màu RGB ──
void handleRgb() {
  if (!guardWeb()) return;
  pauseAutoForApp();
  uint8_t r = clampByte(server.arg("r").toInt());
  uint8_t g = clampByte(server.arg("g").toInt());
  uint8_t b = clampByte(server.arg("b").toInt());
  setRgbAuto(false);
  setRgb(r, g, b);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Đèn thở ──
void handleBreatheLed() {
  if (!guardWeb()) return;
  bool want = !sys.breatheLedState;
  setBreatheAuto(false);
  setBreatheLed(want);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Bật/tắt LED từ xa (hoạt động 12: GET /on và GET /off) ──
// Thao tác thủ công sẽ tắt chế độ tự động đèn để automation không tranh giành điều khiển.
void handleLedOn() {
  if (!guardWeb()) return;
  setLightAuto(false);
  setBlueLed(true);
  saveSettings();
  server.send(200, "text/plain", "LED ON");
}

void handleLedOff() {
  if (!guardWeb()) return;
  setLightAuto(false);
  setBlueLed(false);
  saveSettings();
  server.send(200, "text/plain", "LED OFF");
}

// ── Servo ──
void handleServo() {
  if (!guardWeb()) return;
  int angle = server.arg("angle").toInt();
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  setWindmill(false);   // điều khiển servo thủ công sẽ nhả cối xay gió và cửa cho ăn
  stopFeed();
  sys.servoAngle = angle;
  servo.write(sys.servoAngle);
  server.send(200, "text/plain", "ok");
}

// ── Cối xay gió (hoạt động 6): servo quay liên tục ──
void handleWindmill() {
  if (!guardWeb()) return;
  bool on = server.arg("on") == "true";
  setWindmill(on);
  server.send(200, "text/plain", on ? "windmill on" : "windmill off");
}

// ── Cửa cho ăn (hoạt động 10): mở máng ăn trong FEED_DURATION ──
void handleFeed() {
  if (!guardWeb()) return;
  startFeed();
  server.send(200, "text/plain", "feeding");
}

// ── Demo hoạt động (HD2-HD12) ──
void handleDemo() {
  if (!guardWeb()) return;
  startDemo();
  server.send(200, "text/plain", "demo started");
}

// ── Ngưỡng: cập nhật ngưỡng điều khiển từ web (giới hạn trong khoảng hợp lệ) ──
void handleThreshold() {
  if (!guardWeb()) return;
  bool changed = false;
  float f = 0.0f;
  if (floatArg("soilLow", &f))  { sys.soilThresholdLow  = constrain(f, 0.0f, 4095.0f); changed = true; }
  if (floatArg("soilHigh", &f)) { sys.soilThresholdHigh = constrain(f, 0.0f, 4095.0f); changed = true; }
  if (floatArg("waterLow", &f)) { sys.waterThresholdLow = constrain(f, 0.0f, 4095.0f); changed = true; }
  if (floatArg("tempHigh", &f)) { sys.tempThresholdHigh = constrain(f, 0.0f, 60.0f);   changed = true; }
  if (floatArg("tempLow", &f))  { sys.tempThresholdLow  = constrain(f, 0.0f, 60.0f);   changed = true; }
  const String lightArg = server.arg("light");
  if (lightArg.length() > 0) {
    sys.lightThreshold = constrain(lightArg.toInt(), 0, 4095);
    changed = true;
  }

  if (!changed) {
    server.send(400, "text/plain", "no params");
    return;
  }
  if (sys.soilThresholdHigh <= sys.soilThresholdLow) {
    sys.soilThresholdHigh = constrain(sys.soilThresholdLow + 50.0f, 0.0f, 4095.0f);
  }
  if (sys.tempThresholdHigh <= sys.tempThresholdLow) {
    sys.tempThresholdHigh = sys.tempThresholdLow + 1.0f;  // không chặn trên: giữ high > low tại mép 60 C
  }
  saveSettings();
  LOG_INFO("Thresholds updated from web");
  server.send(200, "text/plain", "ok");
}

// ── Cấu hình WiFi động (lưu vào Preferences, khởi động lại để áp dụng) ──
void handleWifi() {
  if (!guardWeb()) return;
  const String ssidArg = server.arg("ssid");
  const String passArg = server.arg("password");
  if (ssidArg.length() == 0 && passArg.length() == 0) {
    server.send(400, "text/plain", "no params");
    return;
  }

  // SSID phải nằm vừa trong sys.ssid và không được chỉ toàn khoảng trắng.
  // Nhận bừa rồi strncpy cắt ngắn sẽ ghi xuống NVS một SSID sai: máy restart
  // vào mạng không tồn tại và mất liên lạc hoàn toàn qua WiFi — chỉ cứu được
  // bằng cách nạp lại firmware.
  if (ssidArg.length() > 0) {
    String probe = ssidArg;      // trim() sửa tại chỗ nên cần bản sao
    probe.trim();
    if (ssidArg.length() >= sizeof(sys.ssid) || probe.length() == 0) {
      server.send(400, "text/plain", "bad ssid");
      return;
    }
  }
  if (passArg.length() >= sizeof(sys.password)) {
    server.send(400, "text/plain", "password too long");
    return;
  }

  // "changed" phải so với GIÁ TRỊ ĐANG DÙNG, không phải "có tham số". Trước
  // đây /wifi?ssid=X gửi hai lần cũng hai lần restart: mất 3-4 giây WiFi,
  // đang bật bơm thì cắt mất một chu kỳ, và mỗi lần là một vòng NVS.
  bool changed = false;
  if (ssidArg.length() > 0 && strcmp(sys.ssid, ssidArg.c_str()) != 0) {
    strncpy(sys.ssid, ssidArg.c_str(), sizeof(sys.ssid) - 1);
    sys.ssid[sizeof(sys.ssid) - 1] = '\0';
    changed = true;
  }
  if (passArg.length() > 0 && strcmp(sys.password, passArg.c_str()) != 0) {
    strncpy(sys.password, passArg.c_str(), sizeof(sys.password) - 1);
    sys.password[sizeof(sys.password) - 1] = '\0';
    changed = true;
  }
  if (!changed) {
    server.send(200, "text/plain", "unchanged");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
  sys.gNeedRestart = true;
}

// Nhà máy: mọi ngưỡng, cờ tự động và WiFi về mặc định của bản firmware đang chạy.
// resetSettings() trước đây không có ai gọi, nên một bộ ngưỡng sửa sai
// (soilHigh < soilLow -> bơm không bao giờ bật) chỉ thoát được bằng cách
// nạp lại firmware qua OTA/USB.
void handleReset() {
  if (!guardWeb()) return;
  resetSettings();
  server.send(200, "text/plain", "reset");
  // Restart: SSID/mật khẩu vừa về mặc định build, phải kết nối lại từ đầu.
  sys.gNeedRestart = true;
}

// ── Tương thích app ACEBOTT QE002 (hoạt động 13) ──
// Firmware gốc dùng các endpoint: /config, /all, /door?level=, /led?level=,
// /rgb?r=&g=&b=. Giữ đúng tên khóa JSON để app nhận dữ liệu.
void handleAppConfig() {
  server.send(200, "text/plain", "{\"appCallInterval\":1000}");
}

void handleAppAll() {
  if (!checkToken()) return;
  // KHÔNG gọi pauseAutoForApp() ở đây: /all là route CHỈ ĐỌC mà app hỏi mỗi
  // giây, nên trước đây mở app là automation bị tắt vĩnh viễn (bơm không bao
  // giờ chạy tự động). Chỉ các route ra lệnh mới được tạm dừng automation.

  char buf[256];
  const float hum = isfinite(sys.gHum) ? sys.gHum : 0.0f;
  const float temp = isfinite(sys.gTemp) ? sys.gTemp : 0.0f;
  const int n = snprintf(buf, sizeof(buf),
                         "{\"WaterLevel\":%d,\"Humidity\":%.1f,\"Human\":%d,"
                         "\"SoilMoisture\":%d,\"Temperature\":%.1f,\"Photoresistor\":%d}",
                         sys.gWater, (double)hum, sys.gPir ? 1 : 0,
                         sys.gSoil, (double)temp, sys.gLight);
  if (n < 0 || (size_t)n >= sizeof(buf)) {
    server.send(500, "text/plain", "response too large");
    return;
  }
  server.send(200, "text/plain", buf);
}

void handleAppDoor() {
  if (!guardWeb()) return;
  pauseAutoForApp();
  const int level = server.arg("level").toInt();
  setWindmill(false);
  stopFeed();
  sys.servoAngle = level ? FEED_OPEN_ANGLE : FEED_CLOSE_ANGLE;
  servo.write(sys.servoAngle);
  server.send(200, "text/plain", "ok");
}

void handleAppLed() {
  if (!guardWeb()) return;
  pauseAutoForApp();
  setLightAuto(false);
  setBlueLed(server.arg("level").toInt() != 0);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Lịch sử cảm biến (biểu đồ trên web) ──
// Thân response được dựng TRỌN vào heap rồi ghi ra bằng MỘT lần sendContent.
// Lý do không stream từng mẫu: mỗi sendContent() là một NetworkClient::write(),
// và write() của core chặn tới WIFI_CLIENT_MAX_WRITE_RETRY(10) ×
// WIFI_CLIENT_SELECT_TIMEOUT_US(1 s) = 10 s khi phía nhận không chịu đọc
// (slow-loris chỉ cần gửi GET /history rồi ngừng đọc socket). Đường stream cũ
// thực hiện HISTORY_CAPACITY + 2 lần write ≈ 98 × 10 s, trong khi esp_task_wdt
// phát panic ở 30 s — và loop() dừng thì liên khoá an toàn bơm (cắt relay khi
// bình cạn) cũng dừng theo. Một write duy nhất buộc thời gian chặn ≤ 10 s.
void handleHistory() {
  if (!checkToken()) return;
  const size_t total = historyJsonSize();
  char* buf = (char*)malloc(total + 1);
  if (buf == nullptr) {
    server.send(500, "application/json", "{\"error\":\"history out of memory\"}");
    return;
  }
  const int n = buildHistoryJson(buf, total + 1);
  // historyJsonSize() và buildHistoryJson() đi qua cùng một formatSample, nên
  // hai số phải khớp. Lệch = JSON bị cắt: thà trả 500 còn hơn gửi
  // Content-Length dài hơn thân (trình duyệt treo chờ byte cuối).
  if (n < 0 || (size_t)n != total) {
    free(buf);
    server.send(500, "application/json", "{\"error\":\"history too large\"}");
    return;
  }
  server.setContentLength(total);
  server.send(200, "application/json", "");
  server.sendContent(buf, total);
  free(buf);
}

// ── Thời tiết Internet (Open-Meteo, cập nhật mỗi 15 phút) ──
void handleWeather() {
  if (!checkToken()) return;
  char buf[256];
  const int n = buildWeatherJson(buf, sizeof(buf));
  if (n < 0 || (size_t)n >= sizeof(buf)) {
    server.send(500, "application/json", "{\"error\":\"weather too large\"}");
    return;
  }
  server.send(200, "application/json", buf);
}
