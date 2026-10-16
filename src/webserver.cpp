#include "globals.h"
#include "logger.h"
#include "config.h"
#include "settings.h"
#include "actuators.h"
#include "webserver.h"

// ── Root: HTML page ──
void handleRoot() {
  MOD_DEBUG(Webserver, "GET /");

  String html = R"rawliteral(
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
<h2><i class="fa-solid fa-chart-mixed"></i> Cam bien</h2>
<div id="sensorData">
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-temperature-half"></i> Nhiet do</div><div class="sensor-value" id="valTemp">--</div><div class="sensor-unit">&deg;C</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-water"></i> Do am KK</div><div class="sensor-value" id="valHum">--</div><div class="sensor-unit">%</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-sun"></i> Anh sang</div><div class="sensor-value" id="valLight">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-mound"></i> Do am dat</div><div class="sensor-value" id="valSoil">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-tint"></i> Muc nuoc</div><div class="sensor-value" id="valWater">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-person-walking"></i> Chuyen dong</div><div class="sensor-value" id="valPir">--</div></div>
<div class="sensor-row"><div class="sensor-label"><i class="fa-solid fa-hand-point-up"></i> Cam ung</div><div class="sensor-value" id="valTouch">--</div></div>
</div>
</section>

<section class="card">
<h2><i class="fa-solid fa-sliders"></i> Dieu khien</h2>
<div class="controls">
<div class="control-group">
<h3><i class="fa-solid fa-water"></i> May bom</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('pump')"><i class="fa-solid fa-power-off"></i> Bat/Tat</button>
<button class="btn secondary" onclick="setAutoMode('pump',true)">Tu dong</button>
<button class="btn outline" onclick="setAutoMode('pump',false)">Thu cong</button>
</div>
<div class="badge off" id="badgePump"><i class="fa-solid fa-circle"></i> Tat</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-lightbulb"></i> Den xanh</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('led')"><i class="fa-solid fa-power-off"></i> Bat/Tat</button>
</div>
<div class="badge off" id="badgeLed"><i class="fa-solid fa-circle"></i> Tat</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-fan"></i> Quat</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('fan')"><i class="fa-solid fa-power-off"></i> Bat/Tat</button>
<button class="btn secondary" onclick="setAutoMode('fan',true)">Tu dong</button>
<button class="btn outline" onclick="setAutoMode('fan',false)">Thu cong</button>
</div>
<div class="badge off" id="badgeFan"><i class="fa-solid fa-circle"></i> Tat</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-palette"></i> RGB</h3>
<div class="btn-row">
<button class="btn danger" onclick="setRgb(255,0,0)"><i class="fa-solid fa-circle"></i> Do</button>
<button class="btn primary" onclick="setRgb(0,255,0)"><i class="fa-solid fa-circle"></i> Xanh la</button>
<button class="btn secondary" onclick="setRgb(0,0,255)"><i class="fa-solid fa-circle"></i> Xanh duong</button>
<button class="btn outline" onclick="setRgb(255,255,255)"><i class="fa-solid fa-circle"></i> Trang</button>
<button class="btn danger" onclick="setRgb(0,0,0)"><i class="fa-solid fa-circle-stop"></i> Tat</button>
</div>
<div class="badge on" id="badgeRgbAuto"><i class="fa-solid fa-wand-magic-sparkles"></i> Tu dong: Bat</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-heart-pulse"></i> Den thoi</h3>
<div class="btn-row">
<button class="btn primary" onclick="toggleDevice('breathe')"><i class="fa-solid fa-power-off"></i> Bat/Tat</button>
<button class="btn secondary" onclick="setAutoMode('breathe',true)">Tu dong</button>
<button class="btn outline" onclick="setAutoMode('breathe',false)">Thu cong</button>
</div>
<div class="badge off" id="badgeBreathe"><i class="fa-solid fa-circle"></i> Tat</div>
</div>
<div class="control-group">
<h3><i class="fa-solid fa-circle-arrow-right"></i> Servo</h3>
<div class="slider-group">
<label><span id="servoLabel">0</span> do</label>
<input type="range" id="servoSlider" min="0" max="180" value="0" onchange="setServo(this.value)">
</div>
</div>
</div>
<div class="control-group" style="grid-column:1/-1">
<h3><i class="fa-solid fa-robot"></i> Che do tu dong</h3>
<div class="btn-row">
<button class="btn outline" onclick="setAutoMode('pump',true)"><i class="fa-solid fa-tint"></i> Bom TD</button>
<button class="btn outline" onclick="setAutoMode('pump',false)"><i class="fa-solid fa-hand"></i> Bom TC</button>
<button class="btn outline" onclick="setAutoMode('light',true)"><i class="fa-solid fa-lightbulb"></i> Den TD</button>
<button class="btn outline" onclick="setAutoMode('light',false)"><i class="fa-solid fa-hand"></i> Den TC</button>
<button class="btn outline" onclick="setAutoMode('rgb',true)"><i class="fa-solid fa-palette"></i> RGB TD</button>
<button class="btn outline" onclick="setAutoMode('rgb',false)"><i class="fa-solid fa-hand"></i> RGB TC</button>
<button class="btn outline" onclick="setAutoMode('fan',true)"><i class="fa-solid fa-fan"></i> Quat TD</button>
<button class="btn outline" onclick="setAutoMode('fan',false)"><i class="fa-solid fa-hand"></i> Quat TC</button>
<button class="btn outline" onclick="setAutoMode('breathe',true)"><i class="fa-solid fa-heart-pulse"></i> Thoi TD</button>
<button class="btn outline" onclick="setAutoMode('breathe',false)"><i class="fa-solid fa-hand"></i> Thoi TC</button>
</div>
<div class="status-bar">
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Bom: <span id="statusPumpAuto">Bat</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Den: <span id="statusLightAuto">Bat</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> RGB: <span id="statusRgbAuto">Bat</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Quat: <span id="statusFanAuto">Bat</span></div>
<div class="status-item"><i class="fa-solid fa-toggle-on"></i> Thoi: <span id="statusBreatheAuto">Bat</span></div>
</div>
</div>
</section>
<div class="refresh">Cap nhat moi 2 giay &bull; Smart Farm QE002 ACEBOTT</div>
</div>
<script>
async function fetchData(){
try{
const r=await fetch('/api');
const d=await r.json();
document.getElementById('valTemp').textContent=d.temp?.toFixed(1)??'--';
document.getElementById('valHum').textContent=d.hum?.toFixed(1)??'--';
document.getElementById('valLight').textContent=d.light??'--';
document.getElementById('valSoil').textContent=d.soil??'--';
document.getElementById('valWater').textContent=d.water??'--';
document.getElementById('valPir').textContent=d.pir?'Co':'Khong';
document.getElementById('valTouch').textContent=d.touch?'Cham':'Khong';
const p=document.getElementById('badgePump');p.textContent=d.relay?'Bat':'Tat';p.className='badge '+(d.relay?'on':'off');
const l=document.getElementById('badgeLed');l.textContent=d.led?'Bat':'Tat';l.className='badge '+(d.led?'on':'off');
const f=document.getElementById('badgeFan');f.textContent=d.fan?'Bat':'Tat';f.className='badge '+(d.fan?'on':'off');
const b=document.getElementById('badgeBreathe');b.textContent=d.breathe?'Bat':'Tat';b.className='badge '+(d.breathe?'on':'off');
document.getElementById('statusPumpAuto').textContent=d.pumpAuto?'Bat':'Tat';
document.getElementById('statusLightAuto').textContent=d.lightAuto?'Bat':'Tat';
document.getElementById('statusRgbAuto').textContent=d.rgbAuto?'Bat':'Tat';
document.getElementById('statusFanAuto').textContent=d.fanAuto?'Bat':'Tat';
document.getElementById('statusBreatheAuto').textContent=d.breatheAuto?'Bat':'Tat';
document.getElementById('badgeRgbAuto').textContent=d.rgbAuto?'Tu dong: Bat':'Tu dong: Tat';
document.getElementById('badgeRgbAuto').className='badge '+(d.rgbAuto?'on':'off');
}catch(e){console.error(e)}
}
async function toggleDevice(id){await fetch('/toggle?id='+id);fetchData()}
async function setAutoMode(id,on){await fetch('/auto?id='+id+'&on='+on);fetchData()}
async function setRgb(r,g,b){await fetch('/rgb?r='+r+'&g='+g+'&b='+b);fetchData()}
async function setServo(v){document.getElementById('servoLabel').textContent=v;await fetch('/servo?angle='+v)}
setInterval(fetchData,2000);
fetchData();
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ── API: JSON sensor data (uses filtered globals, no raw sensor reads) ──
void handleApi() {
  MOD_DEBUG(Webserver, "GET /api");

  // Use global filtered values — no raw sensor reads here
  // DHT11 needs 1s between reads; reading here would return stale data
  String json = "{";
  json += "\"temp\":" + String(gTemp, 1) + ",";
  json += "\"hum\":" + String(gHum, 1) + ",";
  json += "\"light\":" + String(gLight) + ",";
  json += "\"soil\":" + String(gSoil) + ",";
  json += "\"water\":" + String(gWater) + ",";
  json += "\"pir\":" + String(gPir ? "true" : "false") + ",";
  json += "\"touch\":" + String(gTouch ? "true" : "false") + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"led\":" + String(blueLedState ? "true" : "false") + ",";
  json += "\"breathe\":" + String(breatheLedState ? "true" : "false") + ",";
  json += "\"pumpAuto\":" + String(pumpAuto ? "true" : "false") + ",";
  json += "\"lightAuto\":" + String(lightAuto ? "true" : "false") + ",";
  json += "\"rgbAuto\":" + String(rgbAuto ? "true" : "false") + ",";
  json += "\"fanAuto\":" + String(fanAuto ? "true" : "false") + ",";
  json += "\"breatheAuto\":" + String(breatheAuto ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// ── Toggle device ──
void handleToggle() {
  String id = server.arg("id");
  if (id == "pump") setRelay(!relayState);
  else if (id == "led") setBlueLed(!blueLedState);
  else if (id == "fan") setFan(!fanState);
  else if (id == "breathe") setBreatheLed(!breatheLedState);
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
  else if (id == "breathe") setBreatheAuto(on);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── RGB color ──
void handleRgb() {
  uint8_t r = server.arg("r").toInt();
  uint8_t g = server.arg("g").toInt();
  uint8_t b = server.arg("b").toInt();
  setRgbAuto(false);
  setRgb(r, g, b);
  saveSettings();
  server.send(200, "text/plain", "ok");
}

// ── Breathe LED ──
void handleBreatheLed() {
  setBreatheLed(!breatheLedState);
  server.send(200, "text/plain", "ok");
}

// ── Servo ──
void handleServo() {
  int angle = server.arg("angle").toInt();
  servoAngle = angle;
  servo.write(servoAngle);
  server.send(200, "text/plain", "ok");
}

// ── Threshold: update control thresholds from web ──
void handleThreshold() {
  if (server.hasArg("soilLow"))  soilThresholdLow  = server.arg("soilLow").toFloat();
  if (server.hasArg("soilHigh")) soilThresholdHigh = server.arg("soilHigh").toFloat();
  if (server.hasArg("waterLow")) waterThresholdLow = server.arg("waterLow").toFloat();
  if (server.hasArg("light"))    lightThreshold    = server.arg("light").toInt();
  if (server.hasArg("tempHigh")) tempThresholdHigh = server.arg("tempHigh").toFloat();
  if (server.hasArg("tempLow"))  tempThresholdLow  = server.arg("tempLow").toFloat();
  saveSettings();
  LOG_INFO("Thresholds updated from web");
  server.send(200, "text/plain", "ok");
}
