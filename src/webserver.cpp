#include "globals.h"
#include "logger.h"
#include "config.h"
#include "settings.h"
#include "actuators.h"
#include "sensors.h"
#include "control.h"
#include "demo.h"
#include "webserver.h"

// ── Trang chủ (lưu trong flash, không copy heap mỗi request) ──
static const char ROOT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="referrer" content="no-referrer">
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
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-hand-point-up"></i> Cảm ứng</div><div class="sensor-value" id="valTouch">--</div></div>
</div>
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
async function pollTick(){
if(!document.hidden){const changed=await fetchData();if(changed){stable=0;pollMs=2000}else{stable++;if(stable>=3)pollMs=5000}}
schedulePoll();
}
document.addEventListener('visibilitychange',()=>{if(!document.hidden){wakePoll();fetchData()}});
wakePoll();
fetchData();
</script>
</body>
</html>
)rawliteral";

// ── Bảo vệ web ──
// 1) Chống CSRF: trình duyệt gửi kèm Origin/Referer khi trang khác gọi vào
//    thiết bị; chỉ chấp nhận yêu cầu cùng nguồn với IP của thiết bị.
// 2) Token tùy chọn: nếu WEB_TOKEN được đặt trong config.h, mọi lệnh đổi
//    trạng thái (và cả /api vì trả nonce + dữ liệu cảm biến) phải kèm đúng
//    token qua header X-Auth-Token; ?token= vẫn chấp nhận cho client tự viết.
//    Trang chủ cũng đọc ?token= một lần rồi tự gửi bằng header, không để
//    token dính vào URL các lệnh phía sau.
static bool sameOriginCStr(const char* url, const char* origin) {
  if (url == nullptr || url[0] == '\0') return true;
  size_t olen = strlen(origin);
  if (strncmp(url, origin, olen) != 0) return false;
  char c = url[olen];
  return c == '\0' || c == '/' || c == ':' || c == '?';
}

// So sánh token kiểu constant-time để không rò rỉ độ dài/ký tự khớp qua thời gian phản hồi.
static bool secretMatches(const char* a, const char* b) {
  size_t la = strlen(a), lb = strlen(b);
  size_t diff = la ^ lb;
  for (size_t i = 0; i < la && i < lb; i++) diff |= (size_t)(a[i] ^ b[i]);
  return diff == 0;
}

// Tiền tố nguồn của thiết bị ("http://<ip>") chỉ đổi khi IP đổi; nhớ lại để
// mỗi request của trang web không phải cấp phát String + snprintf.
static const char* localOriginCached() {
  static IPAddress sCachedIp;
  static char sMe[64] = {0};
  IPAddress ip = WiFi.localIP();
  if (sMe[0] == '\0' || ip != sCachedIp) {
    sCachedIp = ip;
    snprintf(sMe, sizeof(sMe), "http://%s", ip.toString().c_str());
  }
  return sMe;
}

static bool checkSameOrigin() {
  // Đọc mỗi header đúng MỘT lần (WebServer cấp phát String cho mỗi lần đọc);
  // header vắng trả chuỗi rỗng nên không cần hasHeader() gọi thêm.
  const String origin = server.header("Origin");
  const String referer = server.header("Referer");
  const String site = server.header("Sec-Fetch-Site");

  // Client không gửi header nguồn (curl, app nội bộ): không có gì để đối chiếu.
  if (origin.length() == 0 && referer.length() == 0 && site.length() == 0) {
    return true;
  }

  const char* me = localOriginCached();

  if (origin.length() > 0 && !sameOriginCStr(origin.c_str(), me)) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  if (referer.length() > 0 && !sameOriginCStr(referer.c_str(), me)) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  if (strcmp(site.c_str(), "cross-site") == 0) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  return true;
}

static bool checkToken() {
  if (sys.webToken[0] == '\0') return true;
  char token[64] = {0};
  // Đọc mỗi nguồn token ĐÚNG MỘT lần: WebServer cấp phát một String mới cho
  // mỗi lần đọc (và cả cho tham số const String&), nên gọi hasHeader() rồi
  // header() sẽ cấp phát 3 lần cho cùng một giá trị.
  const String hdr = server.header("X-Auth-Token");
  if (hdr.length() > 0) {
    strncpy(token, hdr.c_str(), sizeof(token) - 1);
  } else {
    const String q = server.arg("token");
    if (q.length() > 0) strncpy(token, q.c_str(), sizeof(token) - 1);
  }
  token[sizeof(token) - 1] = '\0';
  if (!secretMatches(token, sys.webToken)) {
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

void handleRoot() {
  MOD_DEBUG(Webserver, "GET /");
  server.sendHeader("X-Frame-Options", "DENY");   // chống nhúng iframe (clickjacking)
  // Trang tĩnh: mọi dữ liệu động đều lấy qua /api, nên trình duyệt được
  // cache lại -> mở trang lần sau không phải tải lại 16 KB.
  // "private" để không lưu vào cache dùng chung (proxy).
  server.sendHeader("Cache-Control", "private, max-age=60");
  server.send_P(200, "text/html", ROOT_HTML);
}

// ── API: dữ liệu cảm biến JSON (dùng biến global đã lọc, không đọc cảm biến thô) ──
// Dựng JSON của /api vào buffer cho trước.
// Trả về số byte CẦN (không tính byte NUL) — giống snprintf. Nếu kết quả >=
// kích thước buffer thì JSON bị cắt cụt.
static int buildApiJson(char* buf, size_t cap) {
  const bool demoOn = demoRunning();   // đọc một lần, tránh gọi lặp 2 chỗ trong snprintf
  return snprintf(buf, cap,
           "{\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,"
           "\"weather\":%d,\"pir\":%s,\"touch\":%s,\"relay\":%s,\"fan\":%s,"
           "\"led\":%s,\"breathe\":%s,\"windmill\":%s,\"feed\":%s,\"demo\":%s,"
           "\"demoStep\":\"%s\",\"pumpAuto\":%s,\"lightAuto\":%s,\"rgbAuto\":%s,"
           "\"fanAuto\":%s,\"breatheAuto\":%s,\"nonce\":%u}",
           (double)sys.gTemp, (double)sys.gHum, (int)sys.gLight, (int)sys.gSoil, (int)sys.gWater,
           (int)weatherStatus(),
           sys.gPir ? "true" : "false", sys.gTouch ? "true" : "false",
           sys.relayState ? "true" : "false", sys.fanState ? "true" : "false",
           sys.blueLedState ? "true" : "false", sys.breatheLedState ? "true" : "false",
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
  MOD_DEBUG(Webserver, "GET /api");

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
  size_t len = strlen(buf);
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

// ── Bật/tắt thiết bị (thao tác thủ công sẽ tắt chế độ tự động tương ứng) ──
void handleToggle() {
  if (!guardWeb()) return;
  char idBuf[16] = {0};
  strncpy(idBuf, server.arg("id").c_str(), sizeof(idBuf) - 1);
  const char* id = idBuf;
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
  char idBuf[16] = {0};
  strncpy(idBuf, server.arg("id").c_str(), sizeof(idBuf) - 1);
  const char* id = idBuf;
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
  MOD_DEBUG(Webserver, "GET /on");
  setLightAuto(false);
  setBlueLed(true);
  saveSettings();
  server.send(200, "text/plain", "LED ON");
}

void handleLedOff() {
  if (!guardWeb()) return;
  MOD_DEBUG(Webserver, "GET /off");
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
  MOD_DEBUG(Webserver, "GET /demo");
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
  bool changed = false;
  const String ssidArg = server.arg("ssid");
  if (ssidArg.length() > 0) {
    strncpy(sys.ssid, ssidArg.c_str(), sizeof(sys.ssid) - 1);
    sys.ssid[sizeof(sys.ssid) - 1] = '\0';
    changed = true;
  }
  const String passArg = server.arg("password");
  if (passArg.length() > 0) {
    strncpy(sys.password, passArg.c_str(), sizeof(sys.password) - 1);
    sys.password[sizeof(sys.password) - 1] = '\0';
    changed = true;
  }
  if (!changed) {
    server.send(400, "text/plain", "no params");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
  sys.gNeedRestart = true;
}
