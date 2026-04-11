#pragma once

static const char WEB_HTML[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Air Monitor</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0b0f1a;--bg2:#111827;--bg3:#1a2236;--bg4:#212d42;
  --line:#1e2d45;--text:#e2e8f0;--sub:#64748b;--sub2:#94a3b8;--r:10px;
  --c-temp:#38bdf8;--c-hum:#a78bfa;--c-co2:#4ade80;--c-tvoc:#fb923c;--c-aqi:#fbbf24;
  --good:#22c55e;--warn:#f59e0b;--bad:#ef4444;
}
html,body{height:100%;background:var(--bg);color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,'Inter','Segoe UI',sans-serif;font-size:14px}
/* Header */
header{display:flex;justify-content:space-between;align-items:center;
  padding:12px 20px;background:var(--bg2);border-bottom:1px solid var(--line);
  position:sticky;top:0;z-index:50}
.hdr-left{display:flex;align-items:center;gap:10px}
.hdr-title{font-size:15px;font-weight:600}
.hdr-badge{background:var(--bg4);border:1px solid var(--line);border-radius:6px;
  padding:4px 10px;font-size:12px;color:var(--sub2);font-family:monospace}
.hdr-badge.live{color:#86efac;border-color:#14532d;background:#0f1f18}
.hdr-badge.connecting{color:#fcd34d;border-color:#92400e;background:#1f1720}
.hdr-badge.offline{color:#cbd5e1;border-color:#475569;background:#172033}
.icon-btn{background:var(--bg3);border:1px solid var(--line);border-radius:8px;
  width:34px;height:34px;display:flex;align-items:center;justify-content:center;
  cursor:pointer;font-size:15px;color:var(--sub2);transition:border-color .2s}
.icon-btn:hover{border-color:var(--sub2);color:var(--text)}
/* Layout */
main{max-width:1100px;margin:0 auto;padding:16px}
/* Metric cards */
.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:12px}
@media(max-width:700px){.metrics{grid-template-columns:1fr 1fr}}
.card{background:var(--bg2);border:1px solid var(--line);border-radius:var(--r);
  padding:14px 16px;position:relative;overflow:hidden;transition:border-color .25s}
.card:hover{border-color:var(--bg4)}
.card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;border-radius:var(--r) var(--r) 0 0}
.card.temp::before{background:var(--c-temp)}.card.hum::before{background:var(--c-hum)}
.card.co2::before{background:var(--c-co2)}.card.tvoc::before{background:var(--c-tvoc)}
.card.aqi-card{grid-column:span 4}
@media(max-width:700px){.card.aqi-card{grid-column:span 2}}
.card-header{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:8px}
.card-label{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:1.2px;color:var(--sub)}
.card-info{width:18px;height:18px;border-radius:50%;background:var(--bg3);border:1px solid var(--line);
  display:flex;align-items:center;justify-content:center;font-size:10px;color:var(--sub);
  cursor:default;position:relative}
.tooltip{display:none;position:absolute;right:0;top:22px;z-index:30;
  background:var(--bg4);border:1px solid var(--line);border-radius:8px;
  padding:10px 12px;min-width:190px;font-size:11px;color:var(--sub2);line-height:1.8;white-space:nowrap}
.card-info:hover .tooltip{display:block}
.card-value{font-size:32px;font-weight:700;line-height:1;margin-bottom:2px}
.card-unit{font-size:11px;color:var(--sub);margin-bottom:8px}
.card.temp .card-value{color:var(--c-temp)}.card.hum .card-value{color:var(--c-hum)}
.card.co2  .card-value{color:var(--c-co2)}.card.tvoc .card-value{color:var(--c-tvoc)}
.level-bar{height:4px;background:var(--bg4);border-radius:2px;overflow:hidden;margin-top:6px}
.level-fill{height:100%;border-radius:2px;transition:width .5s ease}
.level-label{font-size:10px;color:var(--sub);margin-top:4px}
.aqi-row{display:flex;align-items:center;gap:20px}
.aqi-num{font-size:28px;font-weight:700;color:var(--c-aqi);min-width:32px}
.aqi-desc{font-size:13px;font-weight:600;margin-bottom:6px}
.aqi-dots{display:flex;gap:5px}
.aqi-dots span{flex:1;height:8px;border-radius:4px;background:var(--bg4);transition:background .4s}
body.offline .card,
body.offline .chart-card,
body.offline .status-bar,
body.offline #warmup-bar{filter:grayscale(1);opacity:.68}
body.offline .card::before{background:#64748b!important}
body.offline .card-value,
body.offline .aqi-num,
body.offline .aqi-desc,
body.offline .level-label,
body.offline .status-dot span,
body.offline #s-ip,
body.offline #s-ver{color:#94a3b8!important}
/* Warmup */
#warmup-bar{display:none;background:var(--bg2);border:1px solid #92400e;
  border-radius:var(--r);padding:10px 16px;margin-bottom:12px;align-items:center;gap:10px}
#warmup-bar.show{display:flex}
.warmup-progress{flex:1;height:4px;background:var(--bg4);border-radius:2px;overflow:hidden}
.warmup-fill{height:100%;background:#f59e0b;border-radius:2px;transition:width .5s}
/* Charts */
.charts{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}
@media(max-width:600px){.charts{grid-template-columns:1fr}}
.chart-card{background:var(--bg2);border:1px solid var(--line);border-radius:var(--r);padding:14px}
.chart-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.chart-title{display:flex;align-items:center;gap:7px;font-size:11px;font-weight:700;
  text-transform:uppercase;letter-spacing:1px;color:var(--sub2)}
.chart-dot{width:8px;height:8px;border-radius:50%}
.chart-controls{display:flex;gap:4px}
.chart-btn{background:var(--bg4);border:1px solid var(--line);color:var(--sub);
  font-size:10px;font-weight:700;padding:3px 8px;border-radius:5px;cursor:pointer;transition:all .15s}
.chart-btn.active,.chart-btn:hover{background:var(--bg3);border-color:var(--sub2);color:var(--text)}
.chart-wrap{position:relative;height:140px}
.chart-wrap canvas{position:absolute;inset:0}
/* Status bar */
.status-bar{background:var(--bg2);border:1px solid var(--line);border-radius:var(--r);
  padding:10px 16px;display:flex;flex-wrap:wrap;align-items:center;gap:14px}
.status-dot{display:flex;align-items:center;gap:5px;font-size:11px;color:var(--sub2)}
.dot{width:7px;height:7px;border-radius:50%;flex-shrink:0}
.dot.ok{background:var(--good)}.dot.warn{background:var(--warn)}
.dot.err{background:var(--bad)}.dot.off{background:var(--sub)}
.status-divider{width:1px;height:14px;background:var(--line)}
/* Modal */
.overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);
  backdrop-filter:blur(6px);z-index:200;align-items:flex-end;justify-content:center}
.overlay.open{display:flex}
@media(min-width:600px){.overlay.open{align-items:center}}
.modal{background:var(--bg2);border:1px solid var(--line);border-radius:16px 16px 0 0;
  width:100%;max-width:520px;max-height:92vh;overflow-y:auto;padding:20px}
@media(min-width:600px){.modal{border-radius:12px}}
.modal-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:18px}
.modal-hdr h2{font-size:16px;font-weight:700}
.close-btn{background:var(--bg3);border:1px solid var(--line);border-radius:8px;
  width:30px;height:30px;color:var(--sub2);font-size:18px;cursor:pointer;
  display:flex;align-items:center;justify-content:center}
.tab-nav{display:flex;gap:3px;margin-bottom:18px;
  background:var(--bg3);border-radius:8px;padding:3px;flex-wrap:wrap}
.tab-btn{flex:1;background:none;border:none;color:var(--sub);padding:7px 4px;
  border-radius:6px;font-size:11px;font-weight:600;cursor:pointer;
  transition:all .15s;white-space:nowrap;min-width:60px}
.tab-btn.active{background:var(--bg4);color:var(--text)}
.tab-pane{display:none}.tab-pane.active{display:block}
.form-section{margin-bottom:16px}
.form-section-title{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:1px;
  color:var(--sub);margin-bottom:10px;padding-bottom:6px;border-bottom:1px solid var(--line)}
.form-group{margin-bottom:10px}
.form-group label{display:block;font-size:11px;color:var(--sub);margin-bottom:4px}
.form-group input{width:100%;background:var(--bg3);border:1px solid var(--line);
  color:var(--text);padding:9px 12px;border-radius:8px;font-size:13px;outline:none;transition:border-color .2s}
.form-group input:focus{border-color:var(--sub2)}
.form-row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.toggle-row{display:flex;justify-content:space-between;align-items:center;
  padding:8px 0;border-bottom:1px solid var(--line)}
.toggle-row:last-child{border-bottom:none}
.toggle-sub{font-size:10px;color:var(--sub);margin-top:1px}
.toggle{position:relative;width:40px;height:22px;display:inline-block;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:var(--bg4);border-radius:11px;
  cursor:pointer;transition:.25s;border:1px solid var(--line)}
.slider:before{content:'';position:absolute;width:16px;height:16px;
  left:2px;bottom:2px;background:var(--sub);border-radius:50%;transition:.25s}
input:checked+.slider{background:#1d4ed8;border-color:#3b82f6}
input:checked+.slider:before{transform:translateX(18px);background:#fff}
.btn-row{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:14px}
.btn{padding:10px;border-radius:8px;font-size:13px;font-weight:600;
  cursor:pointer;border:none;transition:opacity .2s;text-align:center}
.btn:hover{opacity:.85}
.btn-primary{background:#1d4ed8;color:#fff}
.btn-outline{background:none;border:1px solid var(--line);color:var(--text)}
.btn-warn{background:#92400e;color:#fbbf24}
.btn-danger{background:#7f1d1d;color:#fca5a5}
.btn-green{background:#14532d;color:#86efac}
.btn-full{grid-column:span 2}
/* OTA */
#ota-drop{border:2px dashed var(--line);border-radius:10px;padding:28px;
  text-align:center;cursor:pointer;transition:border-color .2s;margin-bottom:12px}
#ota-drop:hover,#ota-drop.drag{border-color:var(--sub2)}
#ota-drop input{display:none}
.progress-wrap{background:var(--bg3);border-radius:6px;height:8px;overflow:hidden;display:none}
.progress-fill{height:100%;background:#1d4ed8;border-radius:6px;transition:width .2s}
/* Net list */
.net-list{max-height:150px;overflow-y:auto;margin-bottom:10px;display:flex;flex-direction:column;gap:4px}
.net-item{display:flex;justify-content:space-between;align-items:center;
  background:var(--bg3);border:1px solid var(--line);border-radius:7px;
  padding:8px 10px;cursor:pointer;font-size:12px;transition:border-color .15s}
.net-item:hover{border-color:var(--sub2)}
/* Export */
.date-list{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:14px;max-height:120px;overflow-y:auto}
.date-chip{background:var(--bg3);border:1px solid var(--line);border-radius:6px;
  padding:5px 10px;font-size:12px;cursor:pointer;transition:border-color .15s;font-family:monospace}
.date-chip:hover{border-color:var(--c-co2);color:var(--c-co2)}
/* Calibration result */
#cal-result{font-size:12px;color:var(--sub2);margin-top:8px;padding:8px;
  background:var(--bg3);border-radius:6px;display:none}
</style>
</head>
<body>

<header>
  <div class="hdr-left">
    <span style="font-size:18px">🌿</span>
    <span class="hdr-title">Air Monitor</span>
    <div class="hdr-badge" id="hdr-time">--:-- --.--.----</div>
    <div class="hdr-badge connecting" id="hdr-conn">Connecting</div>
  </div>
  <div style="display:flex;gap:8px">
    <div class="icon-btn" onclick="openSettings()">⚙</div>
  </div>
</header>

<main>
<div id="warmup-bar">
  <span id="warmup-text" style="font-size:11px;color:#fbbf24;white-space:nowrap">⏳ ENS160: Starting</span>
  <div class="warmup-progress"><div class="warmup-fill" id="warmup-fill" style="width:0%"></div></div>
</div>

<!-- Metric cards -->
<div class="metrics">
  <div class="card temp">
    <div class="card-header">
      <div class="card-label">Temp</div>
      <div class="card-info">?<div class="tooltip"><b>Температура воздуха</b><br>
        &lt;18°C — холодно<br>18–22°C — комфорт<br>22–26°C — тепло<br>&gt;26°C — жарко</div></div>
    </div>
    <div class="card-value" id="v-temp">--.-</div>
    <div class="card-unit">°C</div>
    <div class="level-bar"><div class="level-fill" id="lv-temp" style="width:0%;background:var(--c-temp)"></div></div>
    <div class="level-label" id="ll-temp">—</div>
  </div>
  <div class="card hum">
    <div class="card-header">
      <div class="card-label">Hum</div>
      <div class="card-info">?<div class="tooltip"><b>Относительная влажность</b><br>
        &lt;30% — сухо<br>30–60% — норма<br>60–70% — повышенная<br>&gt;70% — высокая</div></div>
    </div>
    <div class="card-value" id="v-hum">--.-</div>
    <div class="card-unit">% RH</div>
    <div class="level-bar"><div class="level-fill" id="lv-hum" style="width:0%;background:var(--c-hum)"></div></div>
    <div class="level-label" id="ll-hum">—</div>
  </div>
  <div class="card co2">
    <div class="card-header">
      <div class="card-label">eCO2</div>
      <div class="card-info">?<div class="tooltip"><b>CO₂ эквивалент</b><br>
        400–600 — отлично<br>600–1000 — хорошо<br>1000–1500 — умеренно<br>
        1500–2000 — плохо<br>&gt;2000 — опасно</div></div>
    </div>
    <div class="card-value" id="v-co2">---</div>
    <div class="card-unit">ppm</div>
    <div class="level-bar"><div class="level-fill" id="lv-co2" style="width:0%;background:var(--c-co2)"></div></div>
    <div class="level-label" id="ll-co2">—</div>
  </div>
  <div class="card tvoc">
    <div class="card-header">
      <div class="card-label">TVOC</div>
      <div class="card-info">?<div class="tooltip"><b>Летучие орг. соединения</b><br>
        0–150 — отлично<br>150–500 — хорошо<br>500–1500 — умеренно<br>
        1500–5000 — плохо<br>&gt;5000 — опасно</div></div>
    </div>
    <div class="card-value" id="v-tvoc">---</div>
    <div class="card-unit">ppb</div>
    <div class="level-bar"><div class="level-fill" id="lv-tvoc" style="width:0%;background:var(--c-tvoc)"></div></div>
    <div class="level-label" id="ll-tvoc">—</div>
  </div>
  <div class="card aqi-card">
    <div class="card-header">
      <div class="card-label">Air Quality Index</div>
      <div class="card-info">?<div class="tooltip"><b>Индекс качества воздуха</b><br>
        1 — Excellent (Отличный)<br>2 — Good (Хороший)<br>3 — Moderate (Умеренный)<br>
        4 — Poor (Плохой)<br>5 — Unhealthy (Опасный)</div></div>
    </div>
    <div class="aqi-row">
      <div class="aqi-num" id="v-aqi">-</div>
      <div style="flex:1">
        <div class="aqi-desc" id="aqi-desc">—</div>
        <div class="aqi-dots"><span id="aq1"></span><span id="aq2"></span>
          <span id="aq3"></span><span id="aq4"></span><span id="aq5"></span></div>
      </div>
    </div>
  </div>
</div>

<!-- Charts -->
<div class="charts">
  <div class="chart-card">
    <div class="chart-header">
      <div class="chart-title"><div class="chart-dot" style="background:var(--c-temp)"></div>TEMP</div>
      <div class="chart-controls">
        <button class="chart-btn active" onclick="setRange('temp','A',this)">A</button>
        <button class="chart-btn" onclick="setRange('temp','H',this)">H</button>
        <button class="chart-btn" onclick="setRange('temp','M',this)">M</button>
      </div>
    </div>
    <div class="chart-wrap"><canvas id="cTemp"></canvas></div>
  </div>
  <div class="chart-card">
    <div class="chart-header">
      <div class="chart-title"><div class="chart-dot" style="background:var(--c-hum)"></div>HUM</div>
      <div class="chart-controls">
        <button class="chart-btn active" onclick="setRange('hum','A',this)">A</button>
        <button class="chart-btn" onclick="setRange('hum','H',this)">H</button>
        <button class="chart-btn" onclick="setRange('hum','M',this)">M</button>
      </div>
    </div>
    <div class="chart-wrap"><canvas id="cHum"></canvas></div>
  </div>
  <div class="chart-card">
    <div class="chart-header">
      <div class="chart-title"><div class="chart-dot" style="background:var(--c-co2)"></div>ECO2</div>
      <div class="chart-controls">
        <button class="chart-btn active" onclick="setRange('co2','A',this)">A</button>
        <button class="chart-btn" onclick="setRange('co2','H',this)">H</button>
        <button class="chart-btn" onclick="setRange('co2','M',this)">M</button>
      </div>
    </div>
    <div class="chart-wrap"><canvas id="cCO2"></canvas></div>
  </div>
  <div class="chart-card">
    <div class="chart-header">
      <div class="chart-title"><div class="chart-dot" style="background:var(--c-tvoc)"></div>TVOC</div>
      <div class="chart-controls">
        <button class="chart-btn active" onclick="setRange('tvoc','A',this)">A</button>
        <button class="chart-btn" onclick="setRange('tvoc','H',this)">H</button>
        <button class="chart-btn" onclick="setRange('tvoc','M',this)">M</button>
      </div>
    </div>
    <div class="chart-wrap"><canvas id="cTVOC"></canvas></div>
  </div>
</div>

<!-- Status bar -->
<div class="status-bar">
  <div class="status-dot"><div class="dot off" id="d-aht"></div><span id="s-aht">AHT21</span></div>
  <div class="status-divider"></div>
  <div class="status-dot"><div class="dot off" id="d-ens"></div><span id="s-ens">ENS160</span></div>
  <div class="status-divider"></div>
  <div class="status-dot"><div class="dot off" id="d-rtc"></div><span id="s-rtc">DS3231</span></div>
  <div class="status-divider"></div>
  <div class="status-dot"><div class="dot off" id="d-sd"></div><span id="s-sd">SD Card</span></div>
  <div class="status-divider"></div>
  <div class="status-dot"><div class="dot off" id="d-mqtt"></div><span id="s-mqtt">MQTT</span></div>
  <div class="status-divider"></div>
  <div class="status-dot"><span id="s-ip" style="color:var(--sub)">IP: ---</span></div>
  <div class="status-divider"></div>
  <div class="status-dot"><span id="s-ver" style="color:var(--sub)">v-</span></div>
</div>
</main>

<!-- Settings overlay -->
<div class="overlay" id="overlay" onclick="bgClose(event)">
<div class="modal">
  <div class="modal-hdr">
    <h2>Settings</h2>
    <button class="close-btn" onclick="closeSettings()">✕</button>
  </div>
  <div class="tab-nav">
    <button class="tab-btn active" onclick="tab('wifi',this)">WiFi</button>
    <button class="tab-btn" onclick="tab('mqtt',this)">MQTT</button>
    <button class="tab-btn" onclick="tab('tg',this)">Telegram</button>
    <button class="tab-btn" onclick="tab('thr',this)">Alerts</button>
    <button class="tab-btn" onclick="tab('export',this)">Export</button>
    <button class="tab-btn" onclick="tab('ota',this)">OTA</button>
    <button class="tab-btn" onclick="tab('sys',this)">System</button>
  </div>

  <!-- WiFi -->
  <div class="tab-pane active" id="tab-wifi">
    <div class="form-section">
      <div class="form-section-title">Available Networks</div>
      <div class="net-list" id="net-list"><div style="color:var(--sub);font-size:12px;padding:8px">Tap Scan...</div></div>
      <button class="btn btn-outline" style="width:100%;margin-bottom:14px" onclick="scanWifi()">🔍 Scan Networks</button>
    </div>
    <div class="form-section">
      <div class="form-section-title">Credentials</div>
      <div class="form-group"><label>SSID</label><input id="cfg-ssid" placeholder="Network name"></div>
      <div class="form-group"><label>Password</label><input id="cfg-pass" type="password" placeholder="Password"></div>
    </div>
    <div class="btn-row">
      <button class="btn btn-primary btn-full" onclick="saveWifi()">💾 Save & Reconnect</button>
    </div>
  </div>

  <!-- MQTT -->
  <div class="tab-pane" id="tab-mqtt">
    <div class="toggle-row" style="margin-bottom:14px">
      <div><div>Enable MQTT</div><div class="toggle-sub">Publish every 30s</div></div>
      <label class="toggle"><input type="checkbox" id="mqtt-en"><span class="slider"></span></label>
    </div>
    <div class="form-row">
      <div class="form-group"><label>Host / IP</label><input id="mqtt-host" placeholder="192.168.1.100"></div>
      <div class="form-group"><label>Port</label><input id="mqtt-port" value="1883" type="number"></div>
    </div>
    <div class="form-row">
      <div class="form-group"><label>Username</label><input id="mqtt-user"></div>
      <div class="form-group"><label>Password</label><input id="mqtt-pass" type="password"></div>
    </div>
    <div class="form-group"><label>Topic prefix</label><input id="mqtt-topic" placeholder="airmonitor"></div>
    <div class="btn-row"><button class="btn btn-primary btn-full" onclick="saveMQTT()">💾 Save</button></div>
  </div>

  <!-- Telegram -->
  <div class="tab-pane" id="tab-tg">
    <div class="toggle-row" style="margin-bottom:14px">
      <div><div>Enable Telegram</div><div class="toggle-sub">Alerts + bot commands</div></div>
      <label class="toggle"><input type="checkbox" id="tg-en"><span class="slider"></span></label>
    </div>
    <div class="form-group"><label>Bot Token</label><input id="tg-token" placeholder="123456:AAF..."></div>
    <div class="form-group"><label>Chat ID</label><input id="tg-chatid" placeholder="-100123456"></div>
    <div class="btn-row">
      <button class="btn btn-outline" onclick="testTG()">📤 Test</button>
      <button class="btn btn-primary" onclick="saveTG()">💾 Save</button>
    </div>
  </div>

  <!-- Alert Thresholds -->
  <div class="tab-pane" id="tab-thr">
    <div class="form-section">
      <div class="form-section-title">Alert Thresholds</div>
      <p style="font-size:11px;color:var(--sub);margin-bottom:12px">
        Telegram sends a message when any value exceeds the threshold.<br>
        Repeat alert cooldown: 5 minutes.
      </p>
      <div class="form-row">
        <div class="form-group">
          <label>CO₂ alert (ppm)</label>
          <input id="thr-co2" type="number" value="1500" min="400" max="5000">
        </div>
        <div class="form-group">
          <label>AQI alert (1–5)</label>
          <input id="thr-aqi" type="number" value="3" min="1" max="5">
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>Temp alert (°C)</label>
          <input id="thr-temp" type="number" value="30" min="-10" max="60" step="0.5">
        </div>
        <div class="form-group">
          <label>Humidity alert (%)</label>
          <input id="thr-hum" type="number" value="75" min="10" max="100">
        </div>
      </div>
    </div>
    <div class="btn-row">
      <button class="btn btn-primary btn-full" onclick="saveThresholds()">💾 Save Thresholds</button>
    </div>
    <div id="thr-status" style="font-size:11px;color:var(--good);text-align:center;margin-top:8px;display:none">
      ✅ Saved!
    </div>
  </div>

  <!-- CSV Export -->
  <div class="tab-pane" id="tab-export">
    <div class="form-section">
      <div class="form-section-title">Export CSV from SD Card</div>
      <p style="font-size:11px;color:var(--sub);margin-bottom:12px">
        Select a date to download the daily log file.
      </p>
      <button class="btn btn-outline" style="width:100%;margin-bottom:12px"
              onclick="loadExportDates()">🔄 Load Available Dates</button>
      <div class="date-list" id="date-list">
        <div style="color:var(--sub);font-size:12px">Click Load to fetch dates from SD card...</div>
      </div>
      <div class="form-group">
        <label>Or enter date manually</label>
        <input id="export-date" placeholder="2026-04-10" style="font-family:monospace">
      </div>
    </div>
    <div class="btn-row">
      <button class="btn btn-green btn-full" onclick="doExport()">⬇ Download CSV</button>
    </div>
    <div id="export-status" style="font-size:11px;color:var(--sub2);text-align:center;margin-top:8px"></div>
  </div>

  <!-- OTA -->
  <div class="tab-pane" id="tab-ota">
    <div class="form-section-title" style="margin-bottom:12px">Firmware Update</div>
    <div id="ota-drop" onclick="document.getElementById('ota-file').click()"
         ondragover="event.preventDefault();this.classList.add('drag')"
         ondragleave="this.classList.remove('drag')"
         ondrop="otaDrop(event)">
      <input type="file" id="ota-file" accept=".bin" onchange="otaSelected(this)">
      <div style="font-size:28px;margin-bottom:8px">📦</div>
      <div style="font-size:13px;color:var(--sub2)">Drop .bin here or click to browse</div>
      <div style="font-size:11px;color:var(--sub);margin-top:4px">Sketch → Export Compiled Binary → .bin</div>
    </div>
    <div class="progress-wrap" id="ota-pw"><div class="progress-fill" id="ota-pf" style="width:0%"></div></div>
    <div id="ota-status" style="font-size:12px;color:var(--sub2);text-align:center;margin-top:8px;min-height:18px"></div>
  </div>

  <!-- System -->
  <div class="tab-pane" id="tab-sys">
    <div class="form-section">
      <div class="form-section-title">Device Info</div>
      <div style="font-size:12px;color:var(--sub2);line-height:2.2">
        Firmware: <span id="sys-ver" style="color:var(--text)">-</span><br>
        IP: <span id="sys-ip" style="color:var(--text)">-</span><br>
        Uptime: <span id="sys-uptime" style="color:var(--text)">-</span>
      </div>
    </div>
    <div class="form-section">
      <div class="form-section-title">ENS160 Calibration</div>
      <p style="font-size:11px;color:var(--sub);margin-bottom:10px">
        Resets internal baseline. Run when air is known-clean<br>
        (window open or outdoors). Sensor re-warms up for ~3 min.
      </p>
      <button class="btn btn-outline" style="width:100%" onclick="doCalibrate()">
        🧪 Calibrate Baseline Now
      </button>
      <div id="cal-result"></div>
    </div>
    <div class="form-section">
      <div class="form-section-title">Options</div>
      <div class="toggle-row">
        <div><div>Debug Mode</div><div class="toggle-sub">Verbose Serial output</div></div>
        <label class="toggle"><input type="checkbox" id="dbg-en" onchange="setDebug(this.checked)"><span class="slider"></span></label>
      </div>
    </div>
    <div class="btn-row" style="margin-top:16px">
      <button class="btn btn-warn" onclick="doReboot()">🔄 Reboot</button>
      <button class="btn btn-danger" onclick="doReset()">⚠️ Factory Reset</button>
    </div>
  </div>

</div>
</div>

<script>
// ═══════════════ Chart setup ═══════════════
const MAX_PTS = 1440;
const store = {temp:{A:[],H:[],M:[]},hum:{A:[],H:[],M:[]},co2:{A:[],H:[],M:[]},tvoc:{A:[],H:[],M:[]}};
const ranges = {temp:'A',hum:'A',co2:'A',tvoc:'A'};

const mkChart = (id,color) => new Chart(document.getElementById(id),{
  type:'line',
  data:{labels:[],datasets:[{data:[],borderColor:color,backgroundColor:color+'22',
    borderWidth:1.5,pointRadius:0,fill:true,tension:.35}]},
  options:{responsive:true,maintainAspectRatio:false,animation:false,
    plugins:{legend:{display:false},tooltip:{
      backgroundColor:'#1a2236',borderColor:'#1e2d45',borderWidth:1,
      titleColor:'#94a3b8',bodyColor:'#e2e8f0',displayColors:false,padding:8
    }},
    scales:{
      x:{display:true,ticks:{color:'#475569',font:{size:9},maxTicksLimit:5,maxRotation:0},
        grid:{color:'#1e2d45'},border:{color:'#1e2d45'}},
      y:{ticks:{color:'#475569',font:{size:9},maxTicksLimit:5},
        grid:{color:'#1e2d45'},border:{color:'#1e2d45'}}
    }
  }
});

const charts = {
  temp:mkChart('cTemp','rgba(56,189,248,1)'),
  hum: mkChart('cHum', 'rgba(167,139,250,1)'),
  co2: mkChart('cCO2', 'rgba(74,222,128,1)'),
  tvoc:mkChart('cTVOC','rgba(251,146,60,1)'),
};

function pushPt(key,label,value){
  const pt={label,value};
  ['A','H','M'].forEach(r=>{
    store[key][r].push(pt);
    if(store[key][r].length>MAX_PTS) store[key][r].shift();
  });
  renderChart(key);
}

function renderChart(key){
  const r=ranges[key];
  let pts=store[key][r];
  if(r==='H') pts=pts.slice(-60);
  if(r==='M') pts=pts.slice(-10);
  const c=charts[key];
  c.data.labels=pts.map(p=>p.label);
  c.data.datasets[0].data=pts.map(p=>p.value);
  c.update('none');
}

function setRange(key,r,btn){
  ranges[key]=r;
  btn.closest('.chart-controls').querySelectorAll('.chart-btn').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active');
  renderChart(key);
}

// ═══════════════ History preload ═══════════════
fetch('/api/history').then(r=>r.json()).then(h=>{
  if(!h||!h.temp) return;
  const n=h.temp.length;
  for(let i=0;i<n;i++){
    const lbl=String(i);
    ['A','H','M'].forEach(r=>{
      if(h.temp[i]!==undefined) store.temp[r].push({label:lbl,value:h.temp[i]});
      if(h.hum[i] !==undefined) store.hum[r].push( {label:lbl,value:h.hum[i]});
      if(h.co2[i] !==undefined) store.co2[r].push(  {label:lbl,value:h.co2[i]});
      if(h.tvoc[i]!==undefined) store.tvoc[r].push( {label:lbl,value:h.tvoc[i]});
    });
  }
  Object.keys(charts).forEach(renderChart);
  console.log('[History] Loaded',n,'points from device');
}).catch(()=>{});

// ═══════════════ Level helpers ═══════════════
function setLevel(id,pct,label){
  document.getElementById('lv-'+id).style.width=Math.min(100,pct)+'%';
  document.getElementById('ll-'+id).textContent=label;
}
const AQI_INFO=[
  {desc:'Excellent',color:'#22c55e'},{desc:'Good',color:'#84cc16'},
  {desc:'Moderate',color:'#f59e0b'},{desc:'Poor',color:'#ef4444'},
  {desc:'Unhealthy',color:'#7c3aed'}
];
function updateAQI(aqi){
  document.getElementById('v-aqi').textContent=aqi;
  const info=AQI_INFO[Math.max(0,Math.min(4,aqi-1))];
  const d=document.getElementById('aqi-desc');
  d.textContent=info.desc; d.style.color=info.color;
  for(let i=1;i<=5;i++) document.getElementById('aq'+i).style.background=i<=aqi?info.color:'var(--bg4)';
}
const tempLv=t=>t<10?{p:t/10*20,l:'❄ Very Cold'}:t<18?{p:20+(t-10)/8*20,l:'🥶 Cold'}:
  t<22?{p:40+(t-18)/4*20,l:'✅ Comfortable'}:t<26?{p:60+(t-22)/4*20,l:'🌤 Warm'}:{p:80,l:'🔥 Hot'};
const humLv=h=>h<30?{p:h/30*30,l:'💨 Dry'}:h<60?{p:30+(h-30)/30*40,l:'✅ Normal'}:
  h<70?{p:70,l:'💧 Elevated'}:{p:85,l:'🌊 High'};
const co2Lv=c=>c<600?{p:c/600*20,l:'✅ Excellent'}:c<1000?{p:20+(c-600)/400*30,l:'🟢 Good'}:
  c<1500?{p:50+(c-1000)/500*25,l:'🟡 Moderate'}:c<2000?{p:75,l:'🔴 Poor'}:{p:90,l:'☠ Dangerous'};
const tvocLv=v=>v<150?{p:v/150*20,l:'✅ Excellent'}:v<500?{p:20+(v-150)/350*30,l:'🟢 Good'}:
  v<1500?{p:50+(v-500)/1000*25,l:'🟡 Moderate'}:v<5000?{p:75,l:'🔴 Poor'}:{p:95,l:'☠ Dangerous'};

const ENS_ST=['Operating ok','Warm-up','Initial Start-up','No valid output'];

// ═══════════════ WebSocket ═══════════════
let ws,reconnT;
let lastDataTs=0;
let isOffline=true;
const DATA_TIMEOUT_MS=10000;
const pageStartTs=Date.now();
function setConnBadge(state){
  const el=document.getElementById('hdr-conn');
  if(!el) return;
  el.className='hdr-badge '+state;
  el.textContent=state==='live'?'Live':state==='connecting'?'Connecting':'Offline';
}
function setOfflineState(next){
  if(isOffline===next) return;
  isOffline=next;
  document.body.classList.toggle('offline',next);
}
function markDataAlive(){
  lastDataTs=Date.now();
  setOfflineState(false);
  setConnBadge('live');
}
function checkDataTimeout(){
  const sinceTs=lastDataTs||pageStartTs;
  if(Date.now()-sinceTs>DATA_TIMEOUT_MS){
    setOfflineState(true);
    setConnBadge('offline');
  }
}
function connectWS(){
  setConnBadge(lastDataTs?'offline':'connecting');
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen=()=>{clearTimeout(reconnT);setConnBadge(lastDataTs?'offline':'connecting');};
  ws.onmessage=e=>handleMsg(JSON.parse(e.data));
  ws.onclose=()=>{
    if(Date.now()-lastDataTs>DATA_TIMEOUT_MS || !lastDataTs){
      setOfflineState(true);
      setConnBadge('offline');
    }
    reconnT=setTimeout(connectWS,3000);
  };
  ws.onerror=()=>ws.close();
}
function handleMsg(d){
  if(d.type==='data')     updateData(d);
  if(d.type==='settings') applySettings(d);
  if(d.type==='scan')     renderNets(d.networks);
}
function updateData(d){
  markDataAlive();
  document.getElementById('hdr-time').textContent=(d.time_short||'--:--')+'  '+(d.date||'');
  document.getElementById('v-temp').textContent=d.temp.toFixed(1);
  document.getElementById('v-hum').textContent=d.hum.toFixed(1);
  document.getElementById('v-co2').textContent=d.co2;
  document.getElementById('v-tvoc').textContent=d.tvoc;
  updateAQI(d.aqi);

  const tl=tempLv(d.temp);setLevel('temp',tl.p,tl.l);
  const hl=humLv(d.hum);  setLevel('hum', hl.p,hl.l);
  const cl=co2Lv(d.co2);  setLevel('co2', cl.p,cl.l);
  const vl=tvocLv(d.tvoc);setLevel('tvoc',vl.p,vl.l);

  const ensSt=d.ens_status!==undefined?d.ens_status:(d.warmup?1:0);
  const wb=document.getElementById('warmup-bar');
  if(ensSt!==0){
    wb.classList.add('show');
    document.getElementById('warmup-text').textContent='⏳ ENS160: '+(ENS_ST[ensSt]||'...');
    document.getElementById('warmup-fill').style.width=(d.warmup_pct||0)+'%';
  } else wb.classList.remove('show');

  const lbl=d.time_short||'';
  pushPt('temp',lbl,d.temp); pushPt('hum',lbl,d.hum);
  if(ensSt===0){pushPt('co2',lbl,d.co2);pushPt('tvoc',lbl,d.tvoc);}

  setDot('d-aht',d.aht,'s-aht','AHT21');
  const ensLbl='ENS160 ('+ENS_ST[ensSt]+')';
  document.getElementById('d-ens').className='dot '+(ensSt===0?'ok':ensSt===3?'err':'warn');
  document.getElementById('s-ens').textContent=ensLbl;
  setDot('d-rtc',d.rtc,'s-rtc','DS3231');
  if(d.sd){
    document.getElementById('d-sd').className='dot ok';
    document.getElementById('s-sd').textContent=
      'SD Card ('+d.sd_used+'MB/'+d.sd_total+'MB | '+d.sd_pct+'%)';
  } else {
    document.getElementById('d-sd').className='dot err';
    document.getElementById('s-sd').textContent='SD Card (ERR)';
  }
  setDot('d-mqtt',d.mqtt,'s-mqtt','MQTT');
  document.getElementById('s-ip').textContent='IP: '+d.ip;
  document.getElementById('s-ver').textContent='v'+(d.ver||'-');
  document.getElementById('sys-ver').textContent=d.ver||'-';
  document.getElementById('sys-ip').textContent=d.ip;
  document.getElementById('sys-uptime').textContent=fmtUp(d.uptime);
}
function setDot(did,ok,sid,label){
  document.getElementById(did).className='dot '+(ok?'ok':'err');
  document.getElementById(sid).textContent=label+(ok?' OK':' ERR');
}
function fmtUp(s){return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m';}

// ═══════════════ Settings ═══════════════
function openSettings(){
  document.getElementById('overlay').classList.add('open');
  if(ws&&ws.readyState===1) ws.send(JSON.stringify({cmd:'get_settings'}));
}
function closeSettings(){document.getElementById('overlay').classList.remove('open');}
function bgClose(e){if(e.target===document.getElementById('overlay'))closeSettings();}
function tab(name,btn){
  document.querySelectorAll('.tab-pane').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b=>b.classList.remove('active'));
  document.getElementById('tab-'+name).classList.add('active');
  btn.classList.add('active');
}
function applySettings(d){
  if(d.ssid)       document.getElementById('cfg-ssid').value=d.ssid;
  if(d.mqtt_host)  document.getElementById('mqtt-host').value=d.mqtt_host;
  if(d.mqtt_port)  document.getElementById('mqtt-port').value=d.mqtt_port;
  if(d.mqtt_user)  document.getElementById('mqtt-user').value=d.mqtt_user;
  if(d.mqtt_topic) document.getElementById('mqtt-topic').value=d.mqtt_topic;
  document.getElementById('mqtt-en').checked=!!d.mqtt_en;
  document.getElementById('tg-en').checked=!!d.tg_en;
  if(d.tg_chatid)  document.getElementById('tg-chatid').value=d.tg_chatid;
  // Thresholds
  if(d.thr_co2)     document.getElementById('thr-co2').value=d.thr_co2;
  if(d.thr_aqi)     document.getElementById('thr-aqi').value=d.thr_aqi;
  if(d.thr_temp_hi) document.getElementById('thr-temp').value=d.thr_temp_hi;
  if(d.thr_hum_hi)  document.getElementById('thr-hum').value=d.thr_hum_hi;
}
function saveWifi(){
  const ssid=document.getElementById('cfg-ssid').value.trim();
  const pass=document.getElementById('cfg-pass').value;
  if(!ssid){alert('Enter SSID');return;}
  if(!confirm('Save WiFi? Device will reboot.')) return;
  fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid,pass})}).then(()=>alert('Saved — rebooting...'));
}
function scanWifi(){
  document.getElementById('net-list').innerHTML='<div style="color:var(--sub);font-size:12px;padding:8px">Scanning...</div>';
  fetch('/api/scan').then(r=>r.json()).then(renderNets);
}
function renderNets(nets){
  const el=document.getElementById('net-list');
  if(!nets||!nets.length){el.innerHTML='<div style="color:var(--sub);font-size:12px;padding:8px">None found</div>';return;}
  el.innerHTML=nets.sort((a,b)=>b.rssi-a.rssi).map(n=>
    `<div class="net-item" onclick="document.getElementById('cfg-ssid').value='${n.ssid}'">
      <span>${n.ssid}</span><span style="color:var(--sub);font-size:11px">${n.rssi}dBm${n.enc?' 🔒':''}</span>
    </div>`).join('');
}
function saveMQTT(){
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({mqtt_en:document.getElementById('mqtt-en').checked,
      mqtt_host:document.getElementById('mqtt-host').value,
      mqtt_port:+document.getElementById('mqtt-port').value||1883,
      mqtt_user:document.getElementById('mqtt-user').value,
      mqtt_pass:document.getElementById('mqtt-pass').value,
      mqtt_topic:document.getElementById('mqtt-topic').value})
  }).then(()=>alert('MQTT saved'));
}
function saveTG(){
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({tg_en:document.getElementById('tg-en').checked,
      tg_token:document.getElementById('tg-token').value,
      tg_chatid:document.getElementById('tg-chatid').value})
  }).then(()=>alert('Telegram saved'));
}
function testTG(){fetch('/api/tg_test').then(r=>r.text()).then(t=>alert('Response: '+t));}

// ═══════════════ Alert thresholds ═══════════════
function saveThresholds(){
  const body={
    co2: +document.getElementById('thr-co2').value||1500,
    aqi: +document.getElementById('thr-aqi').value||3,
    temp:+document.getElementById('thr-temp').value||30,
    hum: +document.getElementById('thr-hum').value||75,
  };
  fetch('/api/thresholds',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(body)}).then(r=>{
    if(r.ok){
      const st=document.getElementById('thr-status');
      st.style.display='block';
      setTimeout(()=>st.style.display='none',2000);
    }
  });
}

// ═══════════════ CSV Export ═══════════════
function loadExportDates(){
  fetch('/api/export').then(r=>r.json()).then(dates=>{
    const el=document.getElementById('date-list');
    if(!dates||!dates.length){
      el.innerHTML='<div style="color:var(--sub);font-size:12px">No log files on SD card</div>';return;
    }
    el.innerHTML=dates.sort().reverse().map(d=>
      `<div class="date-chip" onclick="document.getElementById('export-date').value='${d}'">${d}</div>`
    ).join('');
  });
}
function doExport(){
  const date=document.getElementById('export-date').value.trim();
  if(!date){alert('Select or enter a date');return;}
  document.getElementById('export-status').textContent='Downloading...';
  // Trigger file download
  const a=document.createElement('a');
  a.href='/api/export?date='+encodeURIComponent(date);
  a.download='airmonitor_'+date+'.csv';
  a.click();
  setTimeout(()=>document.getElementById('export-status').textContent='',2000);
}

// ═══════════════ OTA ═══════════════
function otaDrop(e){
  e.preventDefault();
  document.getElementById('ota-drop').classList.remove('drag');
  const f=e.dataTransfer.files[0]; if(f) uploadFirmware(f);
}
function otaSelected(inp){if(inp.files[0])uploadFirmware(inp.files[0]);}
function uploadFirmware(file){
  if(!file.name.endsWith('.bin')){alert('Select a .bin file');return;}
  const pw=document.getElementById('ota-pw');
  const pf=document.getElementById('ota-pf');
  const st=document.getElementById('ota-status');
  pw.style.display='block';pf.style.width='0%';st.textContent='Uploading '+file.name+'...';
  const fd=new FormData();fd.append('firmware',file);
  const xhr=new XMLHttpRequest();
  xhr.upload.onprogress=ev=>{
    if(ev.lengthComputable){const p=Math.round(ev.loaded/ev.total*100);
      pf.style.width=p+'%';st.textContent='Uploading... '+p+'%';}
  };
  xhr.onload=()=>{
    if(xhr.status===200){pf.style.background='#22c55e';st.textContent='✅ Done — rebooting...';}
    else st.textContent='❌ Failed: '+xhr.responseText;
  };
  xhr.open('POST','/do_update');xhr.send(fd);
}

// ═══════════════ System ═══════════════
function doReboot(){if(confirm('Reboot?'))fetch('/api/reboot',{method:'POST'});}
function doReset(){
  if(confirm('⚠️ Factory reset? All settings erased!'))
    fetch('/api/reset',{method:'POST'}).then(()=>alert('Reset — rebooting...'));
}
function setDebug(on){
  fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({debug:on})});
}
function doCalibrate(){
  const el=document.getElementById('cal-result');
  el.style.display='block';el.textContent='⏳ Calibrating...';el.style.color='var(--sub2)';
  fetch('/api/calibrate',{method:'POST'}).then(r=>r.json()).then(d=>{
    el.textContent=(d.ok?'✅ ':' ❌ ')+d.msg;
    el.style.color=d.ok?'var(--good)':'var(--bad)';
  }).catch(()=>{el.textContent='❌ Error contacting device';el.style.color='var(--bad)';});
}

connectWS();
setInterval(checkDataTimeout,1000);
</script>
</body>
</html>
)HTMLEOF";
