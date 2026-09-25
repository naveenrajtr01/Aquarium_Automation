// ---------------------------------------------------------------------------
// Aquarium Lighting Controller
//
// White LED strip (PWM dimmed) + addressable RGB strip, controlled by a
// physical on/off + preset-cycling switch, a web dashboard over WiFi, and a
// DS3231 real-time clock driving a daily evening schedule.
// See README.md in the repository root for wiring and dashboard details.
//
// Required libraries (install via Arduino IDE Library Manager):
//   - Adafruit NeoPixel
//   - RTClib (by Adafruit)
//   - ESP Async WebServer (by ESP32Async / lacamera)
//   - Async TCP (by ESP32Async / dvarrel) - ESPAsyncWebServer's dependency
// Bundled with the ESP32 Arduino core (no install needed):
//   - Preferences.h, Wire.h, WiFi.h, ESPmDNS.h
// ---------------------------------------------------------------------------

#include <WiFi.h>
#include "Config.h"
#include "WhiteStrip.h"
#include "RgbStrip.h"
#include "SwitchInput.h"
#include "Buzzer.h"
#include "RtcManager.h"
#include "StateManager.h"
#include "WebDashboard.h"
#include "Logger.h"

WhiteStrip whiteStrip;
RgbStrip rgbStrip;
SwitchInput switchInput;
Buzzer buzzer;
RtcManager rtcManager;
StateManager stateManager;
WebDashboard webDashboard;

// Temporary diagnostic: a bare WiFiServer bypassing AsyncTCP/ESPAsyncWebServer
// entirely, to tell whether "can't reach port 80" is those libraries or the
// underlying WiFi/TCP stack. Test with: Test-NetConnection <ip> -Port 8081
WiFiServer diagServer(8081);

void setup() {
  Logger::begin(115200);
  Logger::log("Booting Aquarium Lighting controller...");

  whiteStrip.begin();
  Logger::log("White strip ready (GPIO25 PWM)");

  rgbStrip.begin();
  Logger::logf("RGB strip ready (%d pixels on GPIO27)", RGB_LED_COUNT);

  switchInput.begin();
  Logger::logf("Switch ready (GPIO19, currently %s)", switchInput.isClosed() ? "CLOSED/ON" : "OPEN/OFF");

  buzzer.begin();
  buzzer.beep(); // brief power-on chime
  Logger::log("Buzzer ready (GPIO32)");

  rtcManager.begin();

  stateManager.begin(&whiteStrip, &rgbStrip, &buzzer);

  // Boot is always treated as a "long time off" - restore the last preset,
  // never advance it, regardless of which position the switch is in.
  stateManager.applyInitialState(switchInput.isClosed());

  webDashboard.begin(&stateManager, &rtcManager);

  diagServer.begin();
  Logger::log("Diagnostic raw TCP server listening on port 8081");

  Logger::log("Setup complete");
}

void loop() {
  SwitchEvent event = switchInput.update();

  if (event == SwitchEvent::TurnedOff) {
    stateManager.handleSwitchTurnedOff();
  } else if (event == SwitchEvent::TurnedOn) {
    stateManager.handleSwitchTurnedOn();
  }

  buzzer.update();

  rtcManager.update();
  if (rtcManager.consumeScheduledTrigger()) {
    stateManager.handleScheduledTrigger();
  }

  stateManager.update();

  webDashboard.update();

  // Bare-bones reachability probe, independent of AsyncTCP/ESPAsyncWebServer.
  WiFiClient diagClient = diagServer.accept();
  if (diagClient) {
    Logger::logf("Diagnostic TCP client connected from %s", diagClient.remoteIP().toString().c_str());
    diagClient.print("OK\n");
    diagClient.stop();
  }

  // WiFi connects during webDashboard.begin() and may drop/reconnect later;
  // report status+IP/network details on a slow cadence rather than
  // flooding the log. Free heap is included since running WiFi +
  // AsyncWebServer can slowly exhaust it, which would otherwise look like
  // an unexplained "dashboard stopped responding".
  static unsigned long lastWifiLogMs = 0;
  if (millis() - lastWifiLogMs >= 60000UL) {
    lastWifiLogMs = millis();
    bool connected = (WiFi.status() == WL_CONNECTED);
    String ip = connected ? WiFi.localIP().toString() : "Not connected";
    if (connected) {
      Logger::logf(
          "WiFi status: CONNECTED, IP: %s, Gateway: %s, Subnet: %s, RSSI: %ddBm, MAC: %s, FreeHeap: %u",
          ip.c_str(),
          WiFi.gatewayIP().toString().c_str(),
          WiFi.subnetMask().toString().c_str(),
          WiFi.RSSI(),
          WiFi.macAddress().c_str(),
          ESP.getFreeHeap());
    } else {
      Logger::logf("WiFi status: DISCONNECTED, MAC: %s, FreeHeap: %u", WiFi.macAddress().c_str(), ESP.getFreeHeap());
    }
  }
}

