#include "BleController.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Config.h"
#include "Logger.h"

namespace {

StateManager *g_stateManager = nullptr;
RtcManager *g_rtcManager = nullptr;

BLEServer *g_server = nullptr;
BLECharacteristic *g_onOffChar = nullptr;
BLECharacteristic *g_presetChar = nullptr;
BLECharacteristic *g_whiteChar = nullptr;
BLECharacteristic *g_redChar = nullptr;
BLECharacteristic *g_greenChar = nullptr;
BLECharacteristic *g_blueChar = nullptr;
BLECharacteristic *g_setTimeChar = nullptr;
BLECharacteristic *g_logChar = nullptr;
BLECharacteristic *g_ipChar = nullptr;

void writeUint8AndNotify(BLECharacteristic *ch, uint8_t value) {
  ch->setValue(&value, 1);
  ch->notify();
}

void writeStringAndNotify(BLECharacteristic *ch, const String &value) {
  ch->setValue(value);
  ch->notify();
}

// Restart advertising after a client disconnects (the ESP32 BLE stack does
// not do this automatically), so the app can always reconnect.
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    Logger::log("BLE client connected");
  }

  void onDisconnect(BLEServer *server) override {
    Logger::log("BLE client disconnected - resuming advertising");
    server->getAdvertising()->start();
  }
};

// Shared write handler for the four adjustable percentage characteristics.
// `setter` applies the request to StateManager; the characteristic is then
// reset to whatever value StateManager actually holds (unchanged if the
// light was off and the write was rejected).
class PercentWriteCallback : public BLECharacteristicCallbacks {
public:
  PercentWriteCallback(
      const char *name,
      bool (StateManager::*setter)(uint8_t),
      uint8_t (StateManager::*getter)() const)
      : name(name), setter(setter), getter(getter) {}

  void onWrite(BLECharacteristic *characteristic) override {
    if (!g_stateManager) return;

    // ESP32 Arduino BLE library returns Arduino String.
    String value = characteristic->getValue();

    if (value.length() == 0) return;

    uint8_t requested = static_cast<uint8_t>(value[0]);

    bool applied = (g_stateManager->*setter)(requested);

    if (applied) {
      Logger::logf("BLE write applied: %s = %u%%", name, requested);
    } else {
      Logger::logf(
          "BLE write rejected (%s = %u%%) - light is off",
          name, requested);
    }

    writeUint8AndNotify(
        characteristic,
        (g_stateManager->*getter)());
  }

private:
  const char *name;
  bool (StateManager::*setter)(uint8_t);
  uint8_t (StateManager::*getter)() const;
};

// Write-only: forwards a little-endian uint32 Unix epoch to RtcManager. Used
// by the app (with an NTP-accurate phone clock) to set the DS3231 precisely,
// working around upload-latency skew in a compile-time set.
class SetTimeCallback : public BLECharacteristicCallbacks {
public:
  explicit SetTimeCallback(RtcManager *rtcManager)
      : rtcManager(rtcManager) {}

  void onWrite(BLECharacteristic *characteristic) override {
    if (!rtcManager) return;

    // ESP32 Arduino BLE library returns Arduino String.
    String value = characteristic->getValue();

    if (value.length() < 4) return;

    uint32_t epochSeconds =
        (uint32_t)(uint8_t)value[0] |
        ((uint32_t)(uint8_t)value[1] << 8) |
        ((uint32_t)(uint8_t)value[2] << 16) |
        ((uint32_t)(uint8_t)value[3] << 24);

    rtcManager->setEpoch(epochSeconds);
  }

private:
  RtcManager *rtcManager;
};

} // namespace

void BleController::begin(
    StateManager *stateManager,
    RtcManager *rtcManager) {

  g_stateManager = stateManager;
  g_rtcManager = rtcManager;

  BLEDevice::init(BLE_DEVICE_NAME);

  // Allow log lines longer than the default 20-byte notify payload.
  BLEDevice::setMTU(185);

  g_server = BLEDevice::createServer();
  g_server->setCallbacks(new ServerCallbacks());

  // Default handle count (15) isn't enough for 8 characteristics plus their
  // CCCD descriptors (~24 needed) - characteristics past the limit are
  // silently never registered, so they'd be invisible to any BLE client.
  BLEService *service =
      g_server->createService(BLEUUID(SERVICE_UUID), 40);

  g_onOffChar = service->createCharacteristic(
      CHAR_ONOFF_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_onOffChar->addDescriptor(new BLE2902());

  g_presetChar = service->createCharacteristic(
      CHAR_PRESET_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_presetChar->addDescriptor(new BLE2902());

  g_whiteChar = service->createCharacteristic(
      CHAR_WHITE_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_whiteChar->addDescriptor(new BLE2902());

  g_whiteChar->setCallbacks(
      new PercentWriteCallback(
          "White",
          &StateManager::setWhiteBrightnessPercent,
          &StateManager::getWhitePercent));

  g_redChar = service->createCharacteristic(
      CHAR_RED_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_redChar->addDescriptor(new BLE2902());

  g_redChar->setCallbacks(
      new PercentWriteCallback(
          "Red",
          &StateManager::setRedPercent,
          &StateManager::getRedPercent));

  g_greenChar = service->createCharacteristic(
      CHAR_GREEN_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_greenChar->addDescriptor(new BLE2902());

  g_greenChar->setCallbacks(
      new PercentWriteCallback(
          "Green",
          &StateManager::setGreenPercent,
          &StateManager::getGreenPercent));

  g_blueChar = service->createCharacteristic(
      CHAR_BLUE_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_blueChar->addDescriptor(new BLE2902());

  g_blueChar->setCallbacks(
      new PercentWriteCallback(
          "Blue",
          &StateManager::setBluePercent,
          &StateManager::getBluePercent));

  g_setTimeChar = service->createCharacteristic(
      CHAR_SETTIME_UUID,
      BLECharacteristic::PROPERTY_WRITE);

  g_setTimeChar->setCallbacks(
      new SetTimeCallback(g_rtcManager));

  g_logChar = service->createCharacteristic(
      CHAR_LOG_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_logChar->addDescriptor(new BLE2902());

  g_ipChar = service->createCharacteristic(
      CHAR_IP_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY);

  g_ipChar->addDescriptor(new BLE2902());
  writeStringAndNotify(g_ipChar, "Not connected");

  service->start();

  BLEAdvertising *advertising =
      BLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);

  // Preferred connection parameters within Apple's accessory guidelines -
  // without this, iOS frequently tears the connection down shortly after
  // connecting because the default params don't meet its requirements.
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();

  Logger::attachBleCharacteristic(g_logChar);

  Logger::log(
      "BLE advertising as '" BLE_DEVICE_NAME "'");

  refreshAll();
}

void BleController::refreshAll() {
  if (!g_stateManager) return;

  writeUint8AndNotify(
      g_onOffChar,
      g_stateManager->isOn() ? 1 : 0);

  writeUint8AndNotify(
      g_presetChar,
      g_stateManager->getPresetIndex());

  writeUint8AndNotify(
      g_whiteChar,
      g_stateManager->getWhitePercent());

  writeUint8AndNotify(
      g_redChar,
      g_stateManager->getRedPercent());

  writeUint8AndNotify(
      g_greenChar,
      g_stateManager->getGreenPercent());

  writeUint8AndNotify(
      g_blueChar,
      g_stateManager->getBluePercent());
}

void BleController::updateIpAddress(const String &ip) {
  if (!g_ipChar) return;
  writeStringAndNotify(g_ipChar, ip);
}
