#include "globals.h"
#include "logger.h"
#include "config.h"
#include "settings.h"
#include "actuators.h"
#include "sensors.h"
#include "control.h"
#include "demo.h"
#include "webserver.h"

// ── Root page (stored in flash, no per-request heap copy) ──
static const char ROOT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Farm IoT</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
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
async function fetchData(){
try{
const r=await fetch('/api');
const d=await r.json();
window.__sf=d;
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
async function toggleDevice(id){await fetch('/toggle?id='+id);fetchData()}
async function ledSet(on){await fetch(on?'/on':'/off');fetchData()}
async function toggleWindmill(){const s=window.__sf||{};await fetch('/windmill?on='+(s.windmill?'false':'true'));fetchData()}
async function feedNow(){await fetch('/feed');fetchData()}
async function runDemo(){if(!confirm('Chạy trình diễn lần lượt các hoạt động (khoảng 40 giây)?'))return;await fetch('/demo');fetchData()}
async function setAutoMode(id,on){await fetch('/auto?id='+id+'&on='+on);fetchData()}
async function setRgb(r,g,b){await fetch('/rgb?r='+r+'&g='+g+'&b='+b);fetchData()}
async function setServo(v){document.getElementById('servoLabel').textContent=v;await fetch('/servo?angle='+v)}
setInterval(fetchData,2000);
fetchData();
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  MOD_DEBUG(Webserver, "GET /");
  server.send_P(200, "text/html", ROOT_HTML);
}

// ── API: JSON sensor data (uses filtered globals, no raw sensor reads) ──
void handleApi() {
  MOD_DEBUG(Webserver, "GET /api");

  // Use global filtered values — no raw sensor reads here
  // DHT11 needs 1s between reads; reading here would return stale data
  String json;
  json.reserve(520);
  json += "{";
  json += "\"temp\":" + String(gTemp, 1) + ",";
  json += "\"hum\":" + String(gHum, 1) + ",";
  json += "\"light\":" + String(gLight) + ",";
  json += "\"soil\":" + String(gSoil) + ",";
  json += "\"water\":" + String(gWater) + ",";
  json += "\"weather\":" + String((int)weatherStatus()) + ",";
  json += "\"pir\":" + String(gPir ? "true" : "false") + ",";
  json += "\"touch\":" + String(gTouch ? "true" : "false") + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"led\":" + String(blueLedState ? "true" : "false") + ",";
  json += "\"breathe\":" + String(breatheLedState ? "true" : "false") + ",";
  json += "\"windmill\":" + String(windmillOn ? "true" : "false") + ",";
  json += "\"feed\":" + String(feedRunning() ? "true" : "false") + ",";
  json += "\"demo\":" + String(demoRunning() ? "true" : "false") + ",";
  json += "\"demoStep\":\"" + String(demoRunning() ? demoStepName() : "") + "\",";
  json += "\"pumpAuto\":" + String(pumpAuto ? "true" : "false") + ",";
  json += "\"lightAuto\":" + String(lightAuto ? "true" : "false") + ",";
  json += "\"rgbAuto\":" + String(rgbAuto ? "true" : "false") + ",";
  json += "\"fanAuto\":" + String(fanAuto ? "true" : "false") + ",";
  json += "\"breatheAuto\":" + String(breatheAuto ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

static uint8_t clampByte(long v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

// ── Toggle device (manual action disables the matching auto mode) ──
void handleToggle() {
  String id = server.arg("id");
  if (id == "pump") {
    setPumpAuto(false);
    setRelay(!relayState);
  } else if (id == "led") {
    setLightAuto(false);
    setBlueLed(!blueLedState);
  } else if (id == "fan") {
    setFanAuto(false);
    setFan(!fanState);
  } else if (id == "breathe") {
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

// ── Auto mode ──
void handleAuto() {
  String id = server.arg("id");
  bool on = server.arg("on") == "true";
  if (id == "pump") setPumpAuto(on);
  else if (id == "light") setLightAuto(on);
  else if (id == "rgb") setRgbAuto(on);
  else if (id == "fan") setFanAuto(on);
  else if (id == "breathe") {
    setBreatheAuto(on);
    setBreatheLed(on);
  } else {
    server.send(400, "text/plain", "unknown id");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── RGB color ──
void handleRgb() {
  uint8_t r = clampByte(server.arg("r").toInt());
  uint8_t g = clampByte(server.arg("g").toInt());
  uint8_t b = clampByte(server.arg("b").toInt());
  setRgbAuto(false);
  setRgb(r, g, b);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Breathe LED ──
void handleBreatheLed() {
  bool want = !breatheLedState;
  setBreatheAuto(false);
  setBreatheLed(want);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Remote LED on/off (activity 12: GET /on and GET /off) ──
// Manual action disables light auto so the automation does not fight it.
void handleLedOn() {
  MOD_DEBUG(Webserver, "GET /on");
  setLightAuto(false);
  setBlueLed(true);
  saveSettings();
  server.send(200, "text/plain", "LED ON");
}

void handleLedOff() {
  MOD_DEBUG(Webserver, "GET /off");
  setLightAuto(false);
  setBlueLed(false);
  saveSettings();
  server.send(200, "text/plain", "LED OFF");
}

// ── Servo ──
void handleServo() {
  int angle = server.arg("angle").toInt();
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  setWindmill(false);   // manual servo control releases windmill and feed gate
  stopFeed();
  servoAngle = angle;
  servo.write(servoAngle);
  server.send(200, "text/plain", "ok");
}

// ── Windmill (activity 6): continuous servo sweep ──
void handleWindmill() {
  bool on = server.arg("on") == "true";
  setWindmill(on);
  server.send(200, "text/plain", on ? "windmill on" : "windmill off");
}

// ── Feed gate (activity 10): open the bowl for FEED_DURATION ──
void handleFeed() {
  startFeed();
  server.send(200, "text/plain", "feeding");
}

// ── Activity demo (HD2-HD12) ──
void handleDemo() {
  MOD_DEBUG(Webserver, "GET /demo");
  startDemo();
  server.send(200, "text/plain", "demo started");
}

// ── Threshold: update control thresholds from web (clamped to valid range) ──
void handleThreshold() {
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
  saveSettings();
  LOG_INFO("Thresholds updated from web");
  server.send(200, "text/plain", "ok");
}
