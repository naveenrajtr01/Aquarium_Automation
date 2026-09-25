#include "WebDashboard.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"
#include "Presets.h"
#include "Logger.h"

namespace {

StateManager *g_stateManager = nullptr;
RtcManager *g_rtcManager = nullptr;

AsyncWebServer g_server(WEB_SERVER_PORT);
AsyncWebSocket g_logSocket("/ws/logs");

bool ensureAuth(AsyncWebServerRequest *request) {
  if (!request->authenticate(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD)) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

String buildStatusJson() {
  String presetsArr = "[";
  for (uint8_t i = 0; i < NUM_PRESETS; i++) {
    if (i) presetsArr += ",";
    presetsArr += "\"";
    presetsArr += PRESETS[i].name;
    presetsArr += "\"";
  }
  presetsArr += "]";

  String json = "{";
  json += "\"on\":";           json += (g_stateManager->isOn() ? "true" : "false");
  json += ",\"preset\":";      json += g_stateManager->getPresetIndex();
  json += ",\"presets\":";     json += presetsArr;
  json += ",\"white\":";       json += g_stateManager->getWhitePercent();
  json += ",\"red\":";         json += g_stateManager->getRedPercent();
  json += ",\"green\":";       json += g_stateManager->getGreenPercent();
  json += ",\"blue\":";        json += g_stateManager->getBluePercent();
  json += ",\"time\":\"";      json += g_rtcManager->getTimeString();
  json += "\",\"timeValid\":"; json += (g_rtcManager->isTimeValid() ? "true" : "false");
  json += ",\"scheduleHour\":";   json += g_rtcManager->getScheduleHour();
  json += ",\"scheduleMinute\":"; json += g_rtcManager->getScheduleMinute();
  json += ",\"scheduleEnabled\":"; json += (g_rtcManager->isScheduleEnabled() ? "true" : "false");
  json += "}";
  return json;
}

void pushLogToWeb(const String &message) {
  if (g_logSocket.count() > 0) g_logSocket.textAll(message);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Logger::logf("Web log stream: client #%u connected", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Logger::logf("Web log stream: client #%u disconnected", client->id());
  }
}

// Surfaces WiFi link flaps (association drops, DHCP re-lease, etc.) that
// would otherwise look like "the dashboard randomly stops responding" with
// no clue why - reason codes match IDF's wifi_err_reason_t.
void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Logger::log("WiFi event: associated with AP");
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Logger::logf("WiFi event: DISCONNECTED (reason code %u)", info.wifi_sta_disconnected.reason);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Logger::logf("WiFi event: GOT_IP %s", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Logger::log("WiFi event: LOST_IP");
      break;
    default:
      break;
  }
}

const char DASHBOARD_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Koi Tank Controls</title>
<style>
  :root { --accent:#22d3ee; --bg1:#03121a; --bg2:#0b2b3a; --card:rgba(255,255,255,0.06); }
  * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
  body {
    margin:0; font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
    background:linear-gradient(160deg,var(--bg1),var(--bg2) 60%,#082230);
    color:#e6f6fa; min-height:100vh; padding:16px 12px 40px;
  }
  .wrap { max-width:480px; margin:0 auto; }
  h1 { text-align:center; font-size:1.4rem; margin:4px 0 18px; letter-spacing:.5px; }
  h1 span { color:var(--accent); }
  .card {
    background:var(--card); border:1px solid rgba(255,255,255,0.08);
    border-radius:16px; padding:16px; margin-bottom:14px;
    backdrop-filter:blur(6px); box-shadow:0 4px 18px rgba(0,0,0,0.25);
  }
  .card h2 {
    font-size:.72rem; text-transform:uppercase; letter-spacing:1.2px;
    color:#8fd9ea; margin:0 0 12px;
  }
  .status-row { display:flex; align-items:center; justify-content:space-between; margin-bottom:8px; }
  .status-row:last-child { margin-bottom:0; }
  .status-label { color:#9fc7d0; font-size:.85rem; }
  .status-value { font-weight:600; }
  .badge {
    display:inline-block; padding:3px 12px; border-radius:999px; font-size:.8rem; font-weight:700;
  }
  .badge.on { background:#134e2b; color:#5be08a; }
  .badge.off { background:#4a1414; color:#f08787; }
  .presets { display:flex; gap:8px; }
  .presets button {
    flex:1; padding:12px 4px; border-radius:12px; border:1px solid rgba(255,255,255,0.12);
    background:rgba(255,255,255,0.04); color:#e6f6fa; font-size:.85rem; font-weight:600;
  }
  .presets button.active { background:var(--accent); color:#04202a; border-color:var(--accent); }
  .slider-row { margin-bottom:6px; }
  .slider-row .lbl { display:flex; justify-content:space-between; font-size:.85rem; color:#bfe6ee; margin-bottom:6px; }
  input[type=range] { width:100%; -webkit-appearance:none; height:34px; background:transparent; }
  input[type=range]::-webkit-slider-runnable-track { height:8px; border-radius:6px; background:rgba(255,255,255,0.15); }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance:none; width:26px; height:26px; margin-top:-9px; border-radius:50%;
    background:var(--accent); border:2px solid #04202a;
  }
  #hueSlider::-webkit-slider-runnable-track {
    background:linear-gradient(to right, red, yellow, lime, cyan, blue, magenta, red);
  }
  #colorSquare {
    position:relative; width:100%; padding-top:65%; border-radius:12px; margin-bottom:14px;
    background:
      linear-gradient(to top, #000, transparent),
      linear-gradient(to right, #fff, transparent),
      hsl(var(--hue,0), 100%, 50%);
    touch-action:none; overflow:hidden;
  }
  #colorMarker {
    position:absolute; width:20px; height:20px; border-radius:50%;
    border:2px solid #fff; box-shadow:0 0 4px rgba(0,0,0,0.6);
    transform:translate(-50%,-50%); pointer-events:none;
  }
  .swatch-row { display:flex; align-items:center; gap:12px; margin-top:4px; }
  #swatch { width:40px; height:40px; border-radius:10px; border:1px solid rgba(255,255,255,0.2); }
  #rgbReadout { font-size:.8rem; color:#9fc7d0; }
  .schedule-row { display:flex; align-items:center; gap:10px; }
  input[type=time] {
    flex:1; padding:10px; border-radius:10px; border:1px solid rgba(255,255,255,0.15);
    background:rgba(255,255,255,0.05); color:#e6f6fa; font-size:1rem;
  }
  button.action {
    padding:10px 16px; border-radius:10px; border:none; background:var(--accent);
    color:#04202a; font-weight:700;
  }
  .toggle { display:flex; align-items:center; justify-content:space-between; }
  .switch { position:relative; width:48px; height:26px; }
  .switch input { opacity:0; width:0; height:0; }
  .slider-toggle {
    position:absolute; cursor:pointer; inset:0; background:rgba(255,255,255,0.15);
    border-radius:999px; transition:.2s;
  }
  .slider-toggle:before {
    content:""; position:absolute; height:20px; width:20px; left:3px; top:3px;
    background:white; border-radius:50%; transition:.2s;
  }
  input:checked + .slider-toggle { background:var(--accent); }
  input:checked + .slider-toggle:before { transform:translateX(22px); }
  input:disabled, button:disabled { opacity:.45; cursor:not-allowed; }
  .card.locked .lockable { opacity:.35; pointer-events:none; }
  .card.locked { cursor:pointer; }
  #logBox {
    margin-top:12px; background:#01090d; border-radius:10px; padding:10px;
    height:180px; overflow-y:auto; font-family:ui-monospace,Menlo,monospace;
    font-size:.72rem; color:#7be0a8; white-space:pre-wrap; word-break:break-word;
    display:none;
  }
  #logBox.visible { display:block; }
  #toast {
    position:fixed; left:50%; bottom:24px; transform:translate(-50%,20px);
    background:#12242c; border:1px solid rgba(255,255,255,0.15); color:#e6f6fa;
    padding:10px 16px; border-radius:10px; font-size:.85rem; opacity:0;
    transition:.25s; pointer-events:none; max-width:90%; text-align:center;
  }
  #toast.show { opacity:1; transform:translate(-50%,0); }
</style>
</head>
<body>
<div class="wrap">
  <h1>🐟 Koi Tank <span>Controls</span></h1>

  <div class="card">
    <h2>Status</h2>
    <div class="status-row">
      <span class="status-label">Light</span>
      <span id="onOffBadge" class="badge off">OFF</span>
    </div>
    <div class="status-row">
      <span class="status-label">Active preset</span>
      <span id="activePresetLabel" class="status-value">-</span>
    </div>
    <div class="status-row">
      <span class="status-label">Time (ESP32 / DS3231)</span>
      <span id="timeLabel" class="status-value">-</span>
    </div>
    <div class="status-row">
      <span class="status-label"></span>
      <button class="action" id="syncTimeBtn" style="padding:6px 12px;font-size:.75rem;">Sync time from phone</button>
    </div>
  </div>

  <div class="card" id="presetCard">
    <h2>Preset</h2>
    <div class="presets lockable" id="presetButtons"></div>
  </div>

  <div class="card" id="whiteCard">
    <h2>White brightness</h2>
    <div class="slider-row lockable">
      <div class="lbl"><span>Brightness</span><span id="whiteVal">0%</span></div>
      <input type="range" min="0" max="100" id="whiteSlider">
    </div>
  </div>

  <div class="card" id="rgbCard">
    <h2>RGB color</h2>
    <div class="lockable">
      <div id="colorSquare"><div id="colorMarker"></div></div>
      <input type="range" min="0" max="360" id="hueSlider">
      <div class="swatch-row">
        <div id="swatch"></div>
        <div id="rgbReadout">R0 G0 B0</div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>Daily schedule</h2>
    <div class="toggle" style="margin-bottom:10px;">
      <span class="status-label">Enable daily schedule</span>
      <label class="switch">
        <input type="checkbox" id="scheduleEnableToggle">
        <span class="slider-toggle"></span>
      </label>
    </div>
    <div class="schedule-row">
      <input type="time" id="scheduleInput">
      <button class="action" id="saveScheduleBtn">Save</button>
    </div>
  </div>

  <div class="card">
    <h2>Live logs</h2>
    <div class="toggle">
      <span class="status-label">Stream logs from device</span>
      <label class="switch">
        <input type="checkbox" id="logToggle">
        <span class="slider-toggle"></span>
      </label>
    </div>
    <div id="logBox"></div>
  </div>
</div>

<div id="toast"></div>

<script>
const $ = (id) => document.getElementById(id);
let scheduleTouched = false;
let hue = 0, sat = 1, val = 1;
let colorDragging = false;
let ws = null;
let lastKnownTime = null;
let lastKnownAtMs = 0;
let timeValidCached = false;

function toast(msg) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(toast._t);
  toast._t = setTimeout(() => t.classList.remove('show'), 2200);
}

function postForm(path, params) {
  return fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: new URLSearchParams(params)
  }).then(r => r.json().catch(() => ({})));
}

function hsvToRgb(h, s, v) {
  const c = v * s;
  const x = c * (1 - Math.abs((h / 60) % 2 - 1));
  const m = v - c;
  let r=0,g=0,b=0;
  if (h < 60)       { r=c; g=x; b=0; }
  else if (h < 120) { r=x; g=c; b=0; }
  else if (h < 180) { r=0; g=c; b=x; }
  else if (h < 240) { r=0; g=x; b=c; }
  else if (h < 300) { r=x; g=0; b=c; }
  else              { r=c; g=0; b=x; }
  return [Math.round((r+m)*255), Math.round((g+m)*255), Math.round((b+m)*255)];
}

function rgbToHsv(r, g, b) {
  r/=255; g/=255; b/=255;
  const max = Math.max(r,g,b), min = Math.min(r,g,b), d = max-min;
  let h = 0;
  if (d !== 0) {
    if (max === r) h = 60 * (((g-b)/d) % 6);
    else if (max === g) h = 60 * ((b-r)/d + 2);
    else h = 60 * ((r-g)/d + 4);
    if (h < 0) h += 360;
  }
  const s = max === 0 ? 0 : d/max;
  return [h, s, max];
}

function formatDate(d) {
  const p = (n) => String(n).padStart(2,'0');
  return `${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

function tickClock() {
  if (!lastKnownTime) return;
  const displayed = new Date(lastKnownTime.getTime() + (Date.now() - lastKnownAtMs));
  $('timeLabel').textContent = formatDate(displayed) + (timeValidCached ? '' : ' (unset)');
}

function updateColorPreview() {
  const [r,g,b] = hsvToRgb(hue, sat, val);
  $('swatch').style.background = `rgb(${r},${g},${b})`;
  $('rgbReadout').textContent =
    `R${Math.round(r/255*100)} G${Math.round(g/255*100)} B${Math.round(b/255*100)}`;
  $('colorSquare').style.setProperty('--hue', hue);
  const sq = $('colorSquare');
  $('colorMarker').style.left = (sat * sq.clientWidth) + 'px';
  $('colorMarker').style.top = ((1 - val) * sq.clientHeight) + 'px';
}

function sendColor() {
  const [r,g,b] = hsvToRgb(hue, sat, val);
  postForm('/api/color', {
    red: Math.round(r/255*100),
    green: Math.round(g/255*100),
    blue: Math.round(b/255*100)
  }).then(res => {
    if (res.applied === false) toast('Color not applied - light is off');
    refreshStatus();
  });
}

function setupColorSquare() {
  const sq = $('colorSquare');
  function moveTo(clientX, clientY) {
    const rect = sq.getBoundingClientRect();
    let x = Math.min(Math.max(clientX - rect.left, 0), rect.width);
    let y = Math.min(Math.max(clientY - rect.top, 0), rect.height);
    sat = x / rect.width;
    val = 1 - (y / rect.height);
    updateColorPreview();
  }
  sq.addEventListener('pointerdown', e => { colorDragging = true; moveTo(e.clientX, e.clientY); sq.setPointerCapture(e.pointerId); });
  sq.addEventListener('pointermove', e => { if (colorDragging) moveTo(e.clientX, e.clientY); });
  sq.addEventListener('pointerup',   () => { if (colorDragging) { colorDragging = false; sendColor(); } });
  sq.addEventListener('pointercancel', () => { colorDragging = false; });
}

function setupPresets(names) {
  const box = $('presetButtons');
  box.innerHTML = '';
  names.forEach((name, idx) => {
    const btn = document.createElement('button');
    btn.textContent = name;
    btn.dataset.idx = idx;
    btn.onclick = () => postForm('/api/preset', { index: idx })
      .then(res => {
        if (res.applied === false) toast('Preset not applied - light is off');
        refreshStatus();
      });
    box.appendChild(btn);
  });
}

function refreshStatus() {
  fetch('/api/status').then(r => r.json()).then(s => {
    const badge = $('onOffBadge');
    badge.textContent = s.on ? 'ON' : 'OFF';
    badge.className = 'badge ' + (s.on ? 'on' : 'off');

    if (!$('presetButtons').childElementCount) setupPresets(s.presets);
    [...$('presetButtons').children].forEach((btn, idx) => {
      btn.classList.toggle('active', idx === s.preset);
    });
    $('activePresetLabel').textContent = s.presets[s.preset] || '-';

    lastKnownTime = new Date(s.time.replace(' ', 'T'));
    lastKnownAtMs = Date.now();
    timeValidCached = s.timeValid;
    tickClock();

    if (document.activeElement !== $('whiteSlider')) {
      $('whiteSlider').value = s.white;
      $('whiteVal').textContent = s.white + '%';
    }

    if (!colorDragging) {
      const [h, sv, v] = rgbToHsv(
        Math.round(s.red / 100 * 255),
        Math.round(s.green / 100 * 255),
        Math.round(s.blue / 100 * 255));
      hue = h; sat = sv; val = v;
      $('hueSlider').value = Math.round(hue);
      updateColorPreview();
    }

    if (!scheduleTouched) {
      const hh = String(s.scheduleHour).padStart(2,'0');
      const mm = String(s.scheduleMinute).padStart(2,'0');
      $('scheduleInput').value = `${hh}:${mm}`;
    }
    $('scheduleEnableToggle').checked = s.scheduleEnabled;
    $('scheduleInput').disabled = !s.scheduleEnabled;
    $('saveScheduleBtn').disabled = !s.scheduleEnabled;

    ['presetCard','whiteCard','rgbCard'].forEach(id => $(id).classList.toggle('locked', !s.on));
  }).catch(() => {});
}

$('whiteSlider').addEventListener('input', () => {
  $('whiteVal').textContent = $('whiteSlider').value + '%';
});
$('whiteSlider').addEventListener('change', () => {
  postForm('/api/white', { value: $('whiteSlider').value })
    .then(res => {
      if (res.applied === false) toast('Brightness not applied - light is off');
      refreshStatus();
    });
});

$('hueSlider').addEventListener('input', () => { hue = Number($('hueSlider').value); updateColorPreview(); });
$('hueSlider').addEventListener('change', sendColor);

$('scheduleInput').addEventListener('focus', () => scheduleTouched = true);
$('saveScheduleBtn').addEventListener('click', () => {
  const [hh, mm] = $('scheduleInput').value.split(':').map(Number);
  postForm('/api/schedule', { hour: hh, minute: mm }).then(() => {
    scheduleTouched = false;
    toast('Schedule saved');
    refreshStatus();
  });
});

$('scheduleEnableToggle').addEventListener('change', (e) => {
  postForm('/api/schedule-enabled', { enabled: e.target.checked ? 1 : 0 }).then(refreshStatus);
});

$('syncTimeBtn').addEventListener('click', () => {
  postForm('/api/time', { epoch: Math.floor(Date.now() / 1000) }).then(() => {
    toast('Time synced');
    refreshStatus();
  });
});

['presetCard','whiteCard','rgbCard'].forEach(id => {
  $(id).addEventListener('click', () => {
    if ($(id).classList.contains('locked')) toast('Turn on the light to use this control');
  });
});

$('logToggle').addEventListener('change', (e) => {
  const box = $('logBox');
  if (e.target.checked) {
    box.classList.add('visible');
    box.textContent = '';
    ws = new WebSocket(`ws://${location.host}/ws/logs`);
    ws.onmessage = (evt) => {
      box.textContent += evt.data + '\n';
      box.scrollTop = box.scrollHeight;
      const lines = box.textContent.split('\n');
      if (lines.length > 300) box.textContent = lines.slice(-300).join('\n');
    };
  } else {
    if (ws) { ws.close(); ws = null; }
    box.classList.remove('visible');
    box.textContent = '';
  }
});

setupColorSquare();
updateColorPreview();
refreshStatus();
setInterval(refreshStatus, 3000);
setInterval(tickClock, 1000);
</script>
</body>
</html>
)HTML";

} // namespace

void WebDashboard::begin(StateManager *stateManager, RtcManager *rtcManager) {
  g_stateManager = stateManager;
  g_rtcManager = rtcManager;

  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  // Disable WiFi modem sleep (power save). WiFi power-save puts the radio
  // into a low-power receive schedule that can miss/delay incoming TCP SYN
  // packets - the ESP32 still answers ICMP pings (handled differently) but
  // refuses new TCP connections on ports it's actively listening on. This
  // must be set before WiFi.begin().
  WiFi.setSleep(false);
  WiFi.onEvent(onWifiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Logger::logf("Connecting to WiFi '%s'...", WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250); // one-time blocking wait during setup(), not in loop()
  }

  if (WiFi.status() == WL_CONNECTED) {
    Logger::logf("WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
    if (MDNS.begin(DASHBOARD_MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", WEB_SERVER_PORT);
      Logger::logf("Dashboard: http://%s.local/  (or http://%s/)",
                   DASHBOARD_MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
    }
  } else {
    Logger::log("WiFi connect failed/timed out - check WIFI_SSID/WIFI_PASSWORD in Config.h");
  }

  g_logSocket.setAuthentication(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD);
  g_logSocket.onEvent(onWsEvent);
  g_server.addHandler(&g_logSocket);
  Logger::attachWebSink(pushLogToWeb);

  // No-auth reachability probe for debugging "can't reach the dashboard"
  // issues - if this fails from a phone, the problem is network-level
  // (routing/isolation) rather than anything in the app logic below.
  g_server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
    Logger::logf("Ping from %s", request->client()->remoteIP().toString().c_str());
    String json = "{\"pong\":true,\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\",\"rssi\":";
    json += WiFi.RSSI();
    json += ",\"uptimeMs\":";
    json += millis();
    json += ",\"freeHeap\":";
    json += ESP.getFreeHeap();
    json += "}";
    request->send(200, "application/json", json);
  });

  g_server.onNotFound([](AsyncWebServerRequest *request) {
    Logger::logf("Unmatched request: %s %s from %s",
                 request->methodToString(), request->url().c_str(),
                 request->client()->remoteIP().toString().c_str());
    request->send(404, "text/plain", "Not found");
  });

  g_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    request->send(200, "text/html", DASHBOARD_HTML);
  });

  g_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    request->send(200, "application/json", buildStatusJson());
  });

  g_server.on("/api/white", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    bool applied = false;
    if (request->hasParam("value", true)) {
      uint8_t percent = (uint8_t)request->getParam("value", true)->value().toInt();
      applied = g_stateManager->setWhiteBrightnessPercent(percent);
      Logger::logf("Dashboard write: White = %u%% (%s)", percent, applied ? "applied" : "rejected, light off");
    }
    request->send(200, "application/json", applied ? "{\"applied\":true}" : "{\"applied\":false}");
  });

  g_server.on("/api/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    bool applied = false;
    if (request->hasParam("red", true) && request->hasParam("green", true) && request->hasParam("blue", true)) {
      uint8_t r = (uint8_t)request->getParam("red", true)->value().toInt();
      uint8_t g = (uint8_t)request->getParam("green", true)->value().toInt();
      uint8_t b = (uint8_t)request->getParam("blue", true)->value().toInt();
      applied = g_stateManager->setRedPercent(r);
      applied = g_stateManager->setGreenPercent(g) && applied;
      applied = g_stateManager->setBluePercent(b) && applied;
      Logger::logf("Dashboard write: RGB = %u,%u,%u (%s)", r, g, b, applied ? "applied" : "rejected, light off");
    }
    request->send(200, "application/json", applied ? "{\"applied\":true}" : "{\"applied\":false}");
  });

  g_server.on("/api/preset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    bool applied = false;
    if (request->hasParam("index", true)) {
      uint8_t index = (uint8_t)request->getParam("index", true)->value().toInt();
      applied = g_stateManager->setPresetIndex(index);
    }
    request->send(200, "application/json", applied ? "{\"applied\":true}" : "{\"applied\":false}");
  });

  g_server.on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    if (request->hasParam("hour", true) && request->hasParam("minute", true)) {
      uint8_t hour = (uint8_t)request->getParam("hour", true)->value().toInt();
      uint8_t minute = (uint8_t)request->getParam("minute", true)->value().toInt();
      g_rtcManager->setScheduleTime(hour, minute);
    }
    request->send(200, "application/json", "{\"applied\":true}");
  });

  g_server.on("/api/schedule-enabled", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    if (request->hasParam("enabled", true)) {
      bool enabled = request->getParam("enabled", true)->value().toInt() != 0;
      g_rtcManager->setScheduleEnabled(enabled);
    }
    request->send(200, "application/json", "{\"applied\":true}");
  });

  g_server.on("/api/time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    if (request->hasParam("epoch", true)) {
      uint32_t epoch = (uint32_t)request->getParam("epoch", true)->value().toInt();
      g_rtcManager->setEpoch(epoch);
    }
    request->send(200, "application/json", "{\"applied\":true}");
  });

  g_server.begin();
  Logger::log("Web dashboard started");
}

void WebDashboard::update() {
  g_logSocket.cleanupClients();
}
