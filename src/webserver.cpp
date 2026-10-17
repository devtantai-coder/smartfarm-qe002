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
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css" referrerpolicy="no-referrer">
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
const TOKEN=new URLSearchParams(location.search).get('token')||'';
let NONCE=0;
function api(u){
  let sep=u.indexOf('?')>=0?'&':'?';
  u=u+sep+'token='+encodeURIComponent(TOKEN);
  if(NONCE>0)u=u+'&nonce='+NONCE;
  return u
}
async function fetchData(){
try{
const r=await fetch('/api');
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
}catch(e){console.error(e)}
}
async function toggleDevice(id){await fetch(api('/toggle?id='+id));fetchData()}
async function ledSet(on){await fetch(api(on?'/on':'/off'));fetchData()}
async function toggleWindmill(){const s=window.__sf||{};await fetch(api('/windmill?on='+(s.windmill?'false':'true')));fetchData()}
async function feedNow(){await fetch(api('/feed'));fetchData()}
async function runDemo(){if(!confirm('Chạy trình diễn lần lượt các hoạt động (khoảng 40 giây)?'))return;await fetch(api('/demo'));fetchData()}
async function setAutoMode(id,on){await fetch(api('/auto?id='+id+'&on='+on));fetchData()}
async function setRgb(r,g,b){await fetch(api('/rgb?r='+r+'&g='+g+'&b='+b));fetchData()}
async function setServo(v){document.getElementById('servoLabel').textContent=v;await fetch(api('/servo?angle='+v))}
setInterval(fetchData,2000);
fetchData();
</script>
</body>
</html>
)rawliteral";

// ── Bảo vệ web ──
// 1) Chống CSRF: trình duyệt gửi kèm Origin/Referer khi trang khác gọi vào
//    thiết bị; chỉ chấp nhận yêu cầu cùng nguồn với IP của thiết bị.
// 2) Token tùy chọn: nếu WEB_TOKEN được đặt trong config.h, mọi lệnh đổi
//    trạng thái phải kèm đúng token (header X-Auth-Token hoặc ?token=...).
static bool sameOriginCStr(const char* url, const char* origin) {
  if (url == nullptr || url[0] == '\0') return true;
  size_t olen = strlen(origin);
  if (strncmp(url, origin, olen) != 0) return false;
  char c = url[olen];
  return c == '\0' || c == '/' || c == ':' || c == '?';
}

static bool guardWeb() {
  char me[64];
  snprintf(me, sizeof(me), "http://%s", WiFi.localIP().toString().c_str());

  if (server.hasHeader("Origin")) {
    char origin[128];
    strncpy(origin, server.header("Origin").c_str(), sizeof(origin) - 1);
    origin[sizeof(origin) - 1] = '\0';
    if (!sameOriginCStr(origin, me)) {
      server.send(403, "text/plain", "forbidden");
      return false;
    }
  }
  if (server.hasHeader("Referer")) {
    char referer[128];
    strncpy(referer, server.header("Referer").c_str(), sizeof(referer) - 1);
    referer[sizeof(referer) - 1] = '\0';
    if (!sameOriginCStr(referer, me)) {
      server.send(403, "text/plain", "forbidden");
      return false;
    }
  }
  if (server.hasHeader("Sec-Fetch-Site")) {
    char site[32];
    strncpy(site, server.header("Sec-Fetch-Site").c_str(), sizeof(site) - 1);
    site[sizeof(site) - 1] = '\0';
    if (strcmp(site, "cross-site") == 0) {
      server.send(403, "text/plain", "forbidden");
      return false;
    }
  }
  if (webToken[0] != '\0') {
    char token[64];
    if (server.hasHeader("X-Auth-Token")) {
      strncpy(token, server.header("X-Auth-Token").c_str(), sizeof(token) - 1);
    } else if (server.hasArg("token")) {
      strncpy(token, server.arg("token").c_str(), sizeof(token) - 1);
    } else {
      token[0] = '\0';
    }
    token[sizeof(token) - 1] = '\0';
    if (strcmp(token, webToken) != 0) {
      server.send(401, "text/plain", "unauthorized");
      return false;
    }
  }

#if ANTI_REPLAY
  // Chống replay: yêu cầu nonce lớn hơn lần dùng cuối cùng.
  char nonceStr[16] = {0};
  if (server.hasArg("nonce")) {
    strncpy(nonceStr, server.arg("nonce").c_str(), sizeof(nonceStr) - 1);
    nonceStr[sizeof(nonceStr) - 1] = '\0';
  }
  uint32_t nonce = (uint32_t)strtoul(nonceStr, nullptr, 10);
  if (nonce == 0 || nonce <= gLastNonce) {
    server.send(403, "text/plain", "replay");
    return false;
  }
  gLastNonce = nonce;
#endif

  return true;
}

void handleRoot() {
  MOD_DEBUG(Webserver, "GET /");
  server.sendHeader("X-Frame-Options", "DENY");   // chống nhúng iframe (clickjacking)
  server.send_P(200, "text/html", ROOT_HTML);
}

// ── API: dữ liệu cảm biến JSON (dùng biến global đã lọc, không đọc cảm biến thô) ──
void handleApi() {
  MOD_DEBUG(Webserver, "GET /api");

  // Dùng giá trị global đã lọc — không đọc cảm biến thô ở đây
  // (DHT11 cần 1 giây giữa hai lần đọc; đọc thêm sẽ trả dữ liệu cũ)
  // Dựng JSON bằng snprintf vào buffer cố định — không tự nối String trong handler
  // và không cấp phát heap thêm (send() của thư viện vẫn tạo String nội bộ một lần).
  char buf[512];
  snprintf(buf, sizeof(buf),
           "{\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,"
           "\"weather\":%d,\"pir\":%s,\"touch\":%s,\"relay\":%s,\"fan\":%s,"
           "\"led\":%s,\"breathe\":%s,\"windmill\":%s,\"feed\":%s,\"demo\":%s,"
           "\"demoStep\":\"%s\",\"pumpAuto\":%s,\"lightAuto\":%s,\"rgbAuto\":%s,"
           "\"fanAuto\":%s,\"breatheAuto\":%s,\"nonce\":%u}",
           (double)gTemp, (double)gHum, (int)gLight, (int)gSoil, (int)gWater,
           (int)weatherStatus(),
           gPir ? "true" : "false", gTouch ? "true" : "false",
           relayState ? "true" : "false", fanState ? "true" : "false",
           blueLedState ? "true" : "false", breatheLedState ? "true" : "false",
           windmillOn ? "true" : "false", feedRunning() ? "true" : "false",
           demoRunning() ? "true" : "false",
           demoRunning() ? demoStepName() : "",
           pumpAuto ? "true" : "false", lightAuto ? "true" : "false",
           rgbAuto ? "true" : "false", fanAuto ? "true" : "false",
           breatheAuto ? "true" : "false", gLastNonce + 1);

  server.send(200, "application/json", buf);
}

static uint8_t clampByte(long v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

// ── Bật/tắt thiết bị (thao tác thủ công sẽ tắt chế độ tự động tương ứng) ──
void handleToggle() {
  if (!guardWeb()) return;
  char idBuf[16] = {0};
  strncpy(idBuf, server.arg("id").c_str(), sizeof(idBuf) - 1);
  const char* id = idBuf;
  if (strcmp(id, "pump") == 0) {
    setPumpAuto(false);
    setRelay(!relayState);
  } else if (strcmp(id, "led") == 0) {
    setLightAuto(false);
    setBlueLed(!blueLedState);
  } else if (strcmp(id, "fan") == 0) {
    setFanAuto(false);
    setFan(!fanState);
  } else if (strcmp(id, "breathe") == 0) {
    bool want = !breatheLedState;
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
  bool want = !breatheLedState;
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
  servoAngle = angle;
  servo.write(servoAngle);
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
  if (server.hasArg("soilLow"))  { soilThresholdLow  = constrain(server.arg("soilLow").toFloat(),  0.0f, 4095.0f); changed = true; }
  if (server.hasArg("soilHigh")) { soilThresholdHigh = constrain(server.arg("soilHigh").toFloat(), 0.0f, 4095.0f); changed = true; }
  if (server.hasArg("waterLow")) { waterThresholdLow = constrain(server.arg("waterLow").toFloat(), 0.0f, 4095.0f); changed = true; }
  if (server.hasArg("light"))    { lightThreshold    = constrain(server.arg("light").toInt(),      0,    4095);   changed = true; }
  if (server.hasArg("tempHigh")) { tempThresholdHigh = constrain(server.arg("tempHigh").toFloat(), 0.0f, 60.0f);   changed = true; }
  if (server.hasArg("tempLow"))  { tempThresholdLow  = constrain(server.arg("tempLow").toFloat(),  0.0f, 60.0f);   changed = true; }

  if (!changed) {
    server.send(400, "text/plain", "no params");
    return;
  }
  if (soilThresholdHigh <= soilThresholdLow) {
    soilThresholdHigh = constrain(soilThresholdLow + 50.0f, 0.0f, 4095.0f);
  }
  if (tempThresholdHigh <= tempThresholdLow) {
    tempThresholdHigh = tempThresholdLow + 1.0f;  // không chặn trên: giữ high > low tại mép 60 C
  }
  saveSettings();
  LOG_INFO("Thresholds updated from web");
  server.send(200, "text/plain", "ok");
}

// ── Cấu hình WiFi động (lưu vào Preferences, khởi động lại để áp dụng) ──
void handleWifi() {
  if (!guardWeb()) return;
  bool changed = false;
  if (server.hasArg("ssid")) {
    strncpy(ssid, server.arg("ssid").c_str(), sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    changed = true;
  }
  if (server.hasArg("password")) {
    strncpy(password, server.arg("password").c_str(), sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';
    changed = true;
  }
  if (!changed) {
    server.send(400, "text/plain", "no params");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
  gNeedRestart = true;
}
