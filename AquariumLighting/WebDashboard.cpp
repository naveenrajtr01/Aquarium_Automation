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
  if (!WEB_AUTH_ENABLED) return true;
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

// iOS's "Add to Home Screen" fetcher is unreliable with data: URI
// apple-touch-icon <link> hrefs (often shows a blank/transparent icon) -
// it needs an actual fetchable image resource, served below at
// /apple-touch-icon.png. 180x180 PNG, dark navy background with a cyan
// fish silhouette matching the dashboard's theme.
const uint8_t APPLE_TOUCH_ICON_PNG[] PROGMEM = {
  0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00, 0xB4, 0x08, 0x02, 0x00, 0x00, 0x00, 0xB2, 0xAF, 0x91,
  0x65, 0x00, 0x00, 0x04, 0x1D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0xED, 0xDD, 0x3D, 0x4E, 0x5B,
  0x41, 0x18, 0x46, 0xE1, 0x8B, 0x85, 0x58, 0x04, 0x1A, 0xA5, 0x64, 0x0D, 0x14, 0xE9, 0xA9, 0xB2,
  0xDA, 0x54, 0xF4, 0x29, 0x58, 0x47, 0x34, 0x0B, 0x89, 0x44, 0x15, 0x12, 0x1F, 0x33, 0x36, 0xF3,
  0xF3, 0xCD, 0xCC, 0x79, 0xBA, 0xA0, 0x28, 0xF6, 0x7D, 0x39, 0x99, 0x0B, 0xB6, 0x42, 0xEE, 0x1E,
  0x1E, 0x9F, 0x0E, 0xE9, 0x9C, 0xD3, 0xD9, 0x8F, 0x4A, 0xC6, 0xA1, 0x4B, 0x8C, 0x43, 0xC8, 0x38,
  0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84,
  0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C,
  0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43,
  0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8,
  0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38,
  0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84,
  0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C, 0x43, 0xC8, 0x38, 0x84, 0x8C,
  0x43, 0xC8, 0x38, 0xF6, 0x92, 0x5E, 0xDF, 0xCA, 0x7F, 0xB3, 0x71, 0x6C, 0x24, 0x5D, 0x53, 0x86,
  0x71, 0x6C, 0x24, 0x5D, 0x59, 0x86, 0x71, 0xEC, 0x22, 0x5D, 0x5F, 0xC6, 0x71, 0x1C, 0xF7, 0xC7,
  0xC6, 0xD7, 0x9F, 0x5F, 0x9E, 0x8F, 0x0D, 0xA4, 0x9B, 0xCA, 0x98, 0x35, 0x8E, 0x9B, 0xAF, 0xB6,
  0xE4, 0xCF, 0x59, 0xAC, 0x98, 0xF4, 0x85, 0xAD, 0xEE, 0x77, 0x0B, 0xE2, 0xAA, 0x07, 0xCA, 0x33,
  0x87, 0xF2, 0xF5, 0xC5, 0x42, 0xC7, 0xD1, 0x2D, 0x88, 0xF5, 0x42, 0x49, 0x35, 0xA6, 0x8B, 0x18,
  0xC7, 0xF0, 0x26, 0x2E, 0x3F, 0xAB, 0x1C, 0xBE, 0x92, 0x5A, 0x03, 0xC6, 0x8A, 0x23, 0x66, 0x16,
  0x67, 0x9F, 0x64, 0xD8, 0x44, 0x2A, 0x6E, 0x18, 0x25, 0x8E, 0x29, 0xB2, 0x88, 0x9F, 0x48, 0xDD,
  0x19, 0xC7, 0xC7, 0x31, 0x5D, 0x16, 0x61, 0x13, 0xA9, 0xBE, 0xE4, 0xC8, 0x38, 0xA6, 0xCE, 0x22,
  0x5A, 0x22, 0x2D, 0xC6, 0x1C, 0x13, 0xC7, 0x32, 0x59, 0x04, 0x49, 0xA4, 0xD1, 0x9E, 0xBD, 0xE3,
  0x58, 0x32, 0x8B, 0xB1, 0x89, 0xB4, 0x9B, 0xB4, 0xEB, 0x7B, 0x2B, 0xCB, 0x97, 0x31, 0xF6, 0x25,
  0xBB, 0x89, 0xE3, 0xD8, 0xA7, 0x8C, 0x3E, 0xD7, 0x9B, 0x5E, 0xDF, 0x5A, 0x3F, 0x44, 0x8F, 0xDB,
  0xCA, 0x6E, 0x59, 0x74, 0xB8, 0xC5, 0xF4, 0x99, 0xB4, 0xF9, 0xC9, 0xB1, 0x6D, 0x19, 0xED, 0x16,
  0xE8, 0x36, 0x69, 0xDB, 0x38, 0x2C, 0xA3, 0xFA, 0x0E, 0x3D, 0x27, 0x6D, 0x18, 0x87, 0x65, 0x54,
  0x5F, 0xA3, 0xF3, 0xA4, 0xAD, 0xE2, 0xB0, 0x8C, 0xEA, 0x9B, 0xF4, 0x9F, 0xB4, 0x49, 0x1C, 0x96,
  0x51, 0x7D, 0x99, 0x21, 0x93, 0xD6, 0x8F, 0xC3, 0x32, 0xAA, 0xEF, 0x33, 0x6A, 0xD2, 0xF1, 0x6F,
  0xBC, 0xFD, 0xEF, 0xF7, 0x8F, 0xEF, 0x7F, 0xFF, 0xF2, 0xDB, 0xCF, 0x5F, 0xC7, 0xC6, 0xD2, 0xB8,
  0xBF, 0x6C, 0xA7, 0x68, 0x57, 0xF2, 0x4F, 0x19, 0x67, 0x3F, 0x32, 0xBB, 0x54, 0xBC, 0xD2, 0xD8,
  0x63, 0xF8, 0x14, 0xBC, 0x8C, 0xCB, 0x1F, 0x9F, 0x57, 0xFA, 0x6C, 0xAB, 0x0E, 0x2F, 0x80, 0xCE,
  0xF4, 0xEF, 0x56, 0x2E, 0x17, 0xB0, 0x5E, 0x1F, 0x17, 0x0C, 0xCF, 0xA2, 0x72, 0x1C, 0x5F, 0xBC,
  0x9E, 0x92, 0xCF, 0xFD, 0x62, 0x7D, 0x24, 0x58, 0x2C, 0x48, 0x19, 0xB1, 0x4E, 0x0E, 0x1D, 0x91,
  0xCA, 0xA8, 0x16, 0x47, 0xA8, 0x4B, 0x9A, 0x48, 0xFA, 0xB8, 0x5B, 0xB4, 0x19, 0x3D, 0x39, 0xA2,
  0x48, 0xC1, 0xCA, 0xA8, 0x13, 0x47, 0xC0, 0xAB, 0x9A, 0x48, 0x7A, 0x5F, 0x2F, 0xE6, 0x86, 0x9E,
  0x1C, 0xE3, 0xA5, 0x90, 0x65, 0x04, 0x8A, 0xA3, 0xE4, 0x65, 0xD0, 0xCD, 0x5F, 0x2A, 0xDD, 0x37,
  0x8E, 0x4F, 0x3F, 0xF7, 0x96, 0x31, 0x5F, 0x1C, 0x75, 0x8F, 0x44, 0x2A, 0xC0, 0x32, 0x76, 0x3F,
  0x39, 0xA8, 0x03, 0xCB, 0x18, 0x25, 0xE2, 0xBB, 0xB2, 0xD6, 0x10, 0x44, 0xB8, 0x93, 0x43, 0x71,
  0x18, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x9A, 0xC5, 0x11, 0xE4,
  0x87, 0xDA, 0xA8, 0x05, 0x4F, 0x0E, 0x21, 0xE3, 0x10, 0x32, 0x8E, 0xF1, 0x72, 0xD4, 0x5B, 0xF3,
  0x69, 0xE1, 0x6B, 0x9B, 0x42, 0x7E, 0x5F, 0x2F, 0xE6, 0x86, 0x9E, 0x1C, 0x51, 0xE4, 0x78, 0x7D,
  0x9C, 0x56, 0xBD, 0xB0, 0x29, 0xE4, 0x8F, 0xBB, 0x45, 0x9B, 0xD1, 0x93, 0x23, 0x96, 0xFC, 0xF2,
  0x1C, 0x27, 0x91, 0x6A, 0x71, 0xC4, 0xB9, 0xA4, 0x59, 0x64, 0x5E, 0x2C, 0xC8, 0x98, 0x9E, 0x1C,
  0x41, 0xE5, 0x00, 0x7D, 0x9C, 0x16, 0xBB, 0x9E, 0x59, 0xE4, 0x82, 0xAD, 0x86, 0xEF, 0x59, 0xF9,
  0xE4, 0x18, 0x7E, 0x3D, 0x53, 0xC8, 0xC5, 0x2B, 0x8D, 0xDD, 0xD3, 0xDB, 0x4A, 0x74, 0x79, 0x5C,
  0x1F, 0xF5, 0xE3, 0xF0, 0xF0, 0xA8, 0xBE, 0xCF, 0xA8, 0x49, 0x9B, 0x9C, 0x1C, 0xF6, 0x51, 0x7D,
  0x99, 0x21, 0x93, 0xB6, 0xBA, 0xAD, 0xD8, 0x47, 0xF5, 0x4D, 0xFA, 0xBF, 0x04, 0xD2, 0xF0, 0x6B,
  0x0E, 0xFB, 0x68, 0xB1, 0x46, 0xCF, 0x55, 0xDB, 0x7E, 0x41, 0x6A, 0x1F, 0x2D, 0x76, 0xE8, 0xB6,
  0x6A, 0xF3, 0xEF, 0x56, 0xEC, 0x23, 0x37, 0x58, 0xA0, 0xCF, 0xAA, 0x77, 0x0F, 0x8F, 0x4F, 0x9B,
  0xFF, 0xA0, 0x81, 0x76, 0x72, 0xE3, 0x4F, 0xE1, 0x6D, 0x93, 0x96, 0x3F, 0xAB, 0x7E, 0xAF, 0x73,
  0xEC, 0x76, 0x84, 0xE4, 0xF6, 0xD7, 0xDB, 0xFA, 0x21, 0xBA, 0xBE, 0x08, 0xB6, 0x4F, 0x1F, 0xB9,
  0xD7, 0x95, 0x36, 0x7D, 0xA0, 0x7E, 0xB7, 0x95, 0x4D, 0x6E, 0x31, 0x39, 0xFC, 0xFF, 0x0E, 0x59,
  0xFE, 0x0C, 0xC7, 0xC4, 0xB1, 0x64, 0x22, 0x79, 0xF4, 0xB9, 0x58, 0xB8, 0xE7, 0x1C, 0x71, 0x2C,
  0x93, 0x48, 0x1E, 0x9D, 0xC5, 0x55, 0x63, 0xCE, 0x14, 0xC7, 0xD4, 0x89, 0xE4, 0x30, 0x59, 0x94,
  0x2F, 0x39, 0x5F, 0x1C, 0xD3, 0x25, 0x92, 0xE3, 0x65, 0x51, 0x38, 0xE3, 0xAC, 0x71, 0x4C, 0x91,
  0x48, 0x0E, 0x9C, 0x45, 0xC9, 0x86, 0x73, 0xC7, 0x11, 0xB3, 0x92, 0x3C, 0x43, 0x13, 0x25, 0xEB,
  0x2D, 0x12, 0xC7, 0xF0, 0x50, 0xF2, 0x6C, 0x41, 0x94, 0x8C, 0xB6, 0x60, 0x1C, 0xDD, 0x42, 0x99,
  0x3D, 0x88, 0x4F, 0xB7, 0x5A, 0x3C, 0x8E, 0x5A, 0xC5, 0x2C, 0xD6, 0x41, 0xE1, 0x38, 0x3B, 0xC6,
  0xA1, 0xC2, 0x3E, 0x22, 0xBE, 0xF1, 0xA6, 0xB1, 0x6E, 0x38, 0x26, 0x8D, 0x63, 0x23, 0xF9, 0xCA,
  0x3E, 0xBC, 0xAD, 0x08, 0x79, 0x72, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71,
  0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08,
  0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19,
  0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87,
  0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90,
  0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71,
  0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08,
  0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0x08, 0x19, 0x87, 0x90, 0x71, 0xE8, 0x20,
  0x7F, 0x00, 0xDC, 0x14, 0x46, 0x44, 0xD7, 0xB5, 0xF2, 0x57, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
  0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};
const size_t APPLE_TOUCH_ICON_PNG_LEN = sizeof(APPLE_TOUCH_ICON_PNG);

const char DASHBOARD_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Koi Tank Controls</title>
<link rel="icon" href="data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>%F0%9F%90%9F</text></svg>">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
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
  input[type=range]::-webkit-slider-runnable-track {
    height:8px; border-radius:6px;
    background:linear-gradient(to right, var(--accent) var(--fill,0%), rgba(255,255,255,0.15) var(--fill,0%));
  }
  input[type=range]::-moz-range-track {
    height:8px; border-radius:6px; background:rgba(255,255,255,0.15);
  }
  input[type=range]::-moz-range-progress { height:8px; border-radius:6px; background:var(--accent); }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance:none; width:26px; height:26px; margin-top:-9px; border-radius:50%;
    background:var(--accent); border:2px solid #04202a;
  }
  #hueSlider::-webkit-slider-runnable-track {
    background:linear-gradient(to right, red, yellow, lime, cyan, blue, magenta, red);
  }
  #colorSquare {
    position:relative; width:100%; padding-top:38%; border-radius:12px; margin-bottom:14px;
    background:
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
  .rgb-inputs { display:flex; align-items:flex-end; gap:8px; margin-top:12px; }
  .rgb-inputs label {
    display:flex; flex-direction:column; font-size:.7rem; color:#9fc7d0; gap:4px; flex:1;
  }
  .rgb-inputs input[type=number] {
    width:100%; padding:8px; border-radius:8px; border:1px solid rgba(255,255,255,0.15);
    background:rgba(255,255,255,0.05); color:#e6f6fa; font-size:.9rem; text-align:center;
  }
  .rgb-inputs input[type=color] {
    width:44px; height:38px; padding:0; border:none; border-radius:8px; background:transparent; cursor:pointer;
  }
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
  .card.mode-locked .mode-lockable { opacity:.35; pointer-events:none; }
  .card.mode-locked { cursor:pointer; }
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

  <div class="card">
    <h2>Color mode</h2>
    <div class="presets" id="colorModeSegmented">
      <button type="button" data-mode="preset">Preset</button>
      <button type="button" data-mode="quick">Quick colors</button>
      <button type="button" data-mode="gradient">Gradient</button>
    </div>
  </div>

  <div class="card" id="whiteCard">
    <h2>White brightness</h2>
    <div class="slider-row lockable">
      <div class="lbl"><span>Brightness</span><span id="whiteVal">0%</span></div>
      <input type="range" min="0" max="100" id="whiteSlider">
    </div>
  </div>

  <div class="card" id="presetCard">
    <h2>Preset</h2>
    <div class="presets lockable mode-lockable" id="presetButtons"></div>
  </div>

  <div class="card" id="quickColorCard">
    <h2>Quick colors</h2>
    <div class="presets lockable mode-lockable" id="quickColorButtons"></div>
  </div>

  <div class="card" id="rgbCard">
    <h2>RGB color</h2>
    <div class="slider-row lockable">
      <div class="lbl"><span>Brightness</span><span id="rgbBrightVal">0%</span></div>
      <input type="range" min="0" max="100" id="rgbBrightnessSlider">
    </div>
    <div class="lockable mode-lockable">
      <div id="colorSquare"><div id="colorMarker"></div></div>
      <input type="range" min="0" max="360" id="hueSlider">
      <div class="swatch-row">
        <div id="swatch"></div>
        <div id="rgbReadout">R0 G0 B0</div>
      </div>
      <div class="rgb-inputs">
        <label>R<input type="number" min="0" max="255" id="redBox"></label>
        <label>G<input type="number" min="0" max="255" id="greenBox"></label>
        <label>B<input type="number" min="0" max="255" id="blueBox"></label>
        <input type="color" id="colorPicker" title="Pick exact color">
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

function hexFromRgb(r, g, b) {
  return '#' + [r,g,b].map(x => x.toString(16).padStart(2,'0')).join('');
}

function hexToRgb(hex) {
  const v = hex.replace('#','');
  return [parseInt(v.slice(0,2),16), parseInt(v.slice(2,4),16), parseInt(v.slice(4,6),16)];
}

function clamp255(v) { return Math.min(255, Math.max(0, Math.round(Number(v) || 0))); }

function formatDate(d) {
  const p = (n) => String(n).padStart(2,'0');
  return `${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

function tickClock() {
  if (!lastKnownTime) return;
  const displayed = new Date(lastKnownTime.getTime() + (Date.now() - lastKnownAtMs));
  $('timeLabel').textContent = formatDate(displayed) + (timeValidCached ? '' : ' (unset)');
}

function updateSliderFill(el) {
  const pct = (el.value - el.min) / (el.max - el.min) * 100;
  el.style.setProperty('--fill', pct + '%');
}

function updateColorPreview() {
  const [r,g,b] = hsvToRgb(hue, sat, val);
  $('swatch').style.background = `rgb(${r},${g},${b})`;
  $('rgbReadout').textContent =
    `R${Math.round(r/255*100)} G${Math.round(g/255*100)} B${Math.round(b/255*100)}`;
  $('colorSquare').style.setProperty('--hue', hue);
  const sq = $('colorSquare');
  $('colorMarker').style.left = (sat * sq.clientWidth) + 'px';
  $('colorMarker').style.top = '50%';

  $('hueSlider').value = Math.round(hue);
  $('rgbBrightnessSlider').value = Math.round(val * 100);
  $('rgbBrightVal').textContent = Math.round(val * 100) + '%';
  updateSliderFill($('rgbBrightnessSlider'));

  if (![$('redBox'), $('greenBox'), $('blueBox')].includes(document.activeElement)) {
    $('redBox').value = r;
    $('greenBox').value = g;
    $('blueBox').value = b;
  }
  if (document.activeElement !== $('colorPicker')) {
    $('colorPicker').value = hexFromRgb(r, g, b);
  }
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
  function moveTo(clientX) {
    const rect = sq.getBoundingClientRect();
    let x = Math.min(Math.max(clientX - rect.left, 0), rect.width);
    sat = x / rect.width;
    updateColorPreview();
  }
  sq.addEventListener('pointerdown', e => { colorDragging = true; moveTo(e.clientX); sq.setPointerCapture(e.pointerId); });
  sq.addEventListener('pointermove', e => { if (colorDragging) moveTo(e.clientX); });
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

const QUICK_COLORS = [
  { name: 'Gold',       r: 255, g: 215, b: 0 },
  { name: 'Warm White', r: 255, g: 197, b: 143 },
  { name: 'Amber',      r: 255, g: 126, b: 0 },
  { name: 'Sunset',     r: 255, g: 94,  b: 19 },
];

function setColorFromRgb255(r, g, b) {
  const [h, s, v] = rgbToHsv(r, g, b);
  hue = h; sat = s; val = v;
  updateColorPreview();
  sendColor();
}

function setupQuickColors() {
  const box = $('quickColorButtons');
  box.innerHTML = '';
  QUICK_COLORS.forEach(c => {
    const btn = document.createElement('button');
    btn.textContent = c.name;
    btn.style.background = `rgb(${c.r},${c.g},${c.b})`;
    btn.style.color = (c.r*0.299 + c.g*0.587 + c.b*0.114) > 150 ? '#04202a' : '#fff';
    btn.onclick = () => setColorFromRgb255(c.r, c.g, c.b);
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
      updateSliderFill($('whiteSlider'));
    }

    if (!colorDragging && ![$('redBox'), $('greenBox'), $('blueBox'), $('colorPicker')].includes(document.activeElement)) {
      const [h, sv, v] = rgbToHsv(
        Math.round(s.red / 100 * 255),
        Math.round(s.green / 100 * 255),
        Math.round(s.blue / 100 * 255));
      hue = h; sat = sv; val = v;
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

    ['presetCard','whiteCard','rgbCard','quickColorCard'].forEach(id => $(id).classList.toggle('locked', !s.on));
  }).catch(() => {});
}

$('whiteSlider').addEventListener('input', () => {
  $('whiteVal').textContent = $('whiteSlider').value + '%';
  updateSliderFill($('whiteSlider'));
});
$('whiteSlider').addEventListener('change', () => {
  postForm('/api/white', { value: $('whiteSlider').value })
    .then(res => {
      if (res.applied === false) toast('Brightness not applied - light is off');
      refreshStatus();
    });
});

$('hueSlider').addEventListener('input', () => {
  colorDragging = true;
  hue = Number($('hueSlider').value);
  updateColorPreview();
});
$('hueSlider').addEventListener('change', () => { sendColor(); colorDragging = false; });

$('rgbBrightnessSlider').addEventListener('input', () => {
  colorDragging = true;
  val = Number($('rgbBrightnessSlider').value) / 100;
  updateColorPreview();
});
$('rgbBrightnessSlider').addEventListener('change', () => { sendColor(); colorDragging = false; });

function onRgbBoxInput() {
  const r = clamp255($('redBox').value);
  const g = clamp255($('greenBox').value);
  const b = clamp255($('blueBox').value);
  const [h, s, v] = rgbToHsv(r, g, b);
  hue = h; sat = s; val = v;
  updateColorPreview();
}
['redBox','greenBox','blueBox'].forEach(id => {
  $(id).addEventListener('input', onRgbBoxInput);
  $(id).addEventListener('change', () => { onRgbBoxInput(); sendColor(); });
});

$('colorPicker').addEventListener('input', () => {
  colorDragging = true;
  const [r,g,b] = hexToRgb($('colorPicker').value);
  const [h,s,v] = rgbToHsv(r,g,b);
  hue = h; sat = s; val = v;
  updateColorPreview();
});
$('colorPicker').addEventListener('change', () => { sendColor(); colorDragging = false; });

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

['presetCard','whiteCard','rgbCard','quickColorCard'].forEach(id => {
  $(id).addEventListener('click', (e) => {
    const card = $(id);
    if (card.classList.contains('locked') && e.target.closest('.lockable')) {
      toast('Turn on the light to use this control');
    } else if (card.classList.contains('mode-locked') && e.target.closest('.mode-lockable')) {
      toast('Switch color mode to use this control');
    }
  });
});

const COLOR_MODE_KEY = 'colorMode';
let colorMode = localStorage.getItem(COLOR_MODE_KEY);
if (!['preset','gradient','quick'].includes(colorMode)) colorMode = 'preset';

function applyColorMode() {
  $('presetCard').classList.toggle('mode-locked', colorMode !== 'preset');
  $('rgbCard').classList.toggle('mode-locked', colorMode !== 'gradient');
  $('quickColorCard').classList.toggle('mode-locked', colorMode !== 'quick');
  [...$('colorModeSegmented').children].forEach(btn => {
    btn.classList.toggle('active', btn.dataset.mode === colorMode);
  });
}

$('colorModeSegmented').addEventListener('click', (e) => {
  const btn = e.target.closest('button[data-mode]');
  if (!btn) return;
  colorMode = btn.dataset.mode;
  localStorage.setItem(COLOR_MODE_KEY, colorMode);
  applyColorMode();
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
setupQuickColors();
updateColorPreview();
applyColorMode();
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

  if (WEB_AUTH_ENABLED) g_logSocket.setAuthentication(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD);
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

  g_server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(
        200, "image/png", APPLE_TOUCH_ICON_PNG, APPLE_TOUCH_ICON_PNG_LEN);
    request->send(response);
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
      g_stateManager->beep();
    }
    request->send(200, "application/json", "{\"applied\":true}");
  });

  g_server.on("/api/schedule-enabled", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!ensureAuth(request)) return;
    if (request->hasParam("enabled", true)) {
      bool enabled = request->getParam("enabled", true)->value().toInt() != 0;
      g_rtcManager->setScheduleEnabled(enabled);
      g_stateManager->beep();
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
