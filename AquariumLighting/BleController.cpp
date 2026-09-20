#include "BleController.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Config.h"

namespace {

StateManager *g_stateManager = nullptr;

BLEServer *g_server = nullptr;
BLECharacteristic *g_onOffChar = nullptr;
BLECharacteristic *g_presetChar = nullptr;
BLECharacteristic *g_whiteChar = nullptr;
BLECharacteristic *g_redChar = nullptr;
BLECharacteristic *g_greenChar = nullptr;
BLECharacteristic *g_blueChar = nullptr;

void writeUint8AndNotify(BLECharacteristic *ch, uint8_t value) {
  ch->setValue(&value, 1);
  ch->notify();
}

// Restart advertising after a client disconnects (the ESP32 BLE stack does
// not do this automatically), so the app can always reconnect.
class ServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer *server) override {
    server->getAdvertising()->start();
  }
};

// Shared write handler for the four adjustable percentage characteristics.
// `setter` applies the request to StateManager; the characteristic is then
// reset to whatever value StateManager actually holds (unchanged if the
// light was off and the write was rejected).
class PercentWriteCallback : public BLECharacteristicCallbacks {
public:
  PercentWriteCallback(bool (StateManager::*setter)(uint8_t), uint8_t (StateManager::*getter)() const)
    : setter(setter), getter(getter) {}

  void onWrite(BLECharacteristic *characteristic) override {
    if (!g_stateManager) return;
    std::string value = characteristic->getValue();
    if (value.empty()) return;
    uint8_t requested = static_cast<uint8_t>(value[0]);
    (g_stateManager->*setter)(requested);
    writeUint8AndNotify(characteristic, (g_stateManager->*getter)());
  }

private:
  bool (StateManager::*setter)(uint8_t);
  uint8_t (StateManager::*getter)() const;
};

} // namespace

void BleController::begin(StateManager *stateManager) {
  g_stateManager = stateManager;

  BLEDevice::init(BLE_DEVICE_NAME);
  g_server = BLEDevice::createServer();
  g_server->setCallbacks(new ServerCallbacks());

  BLEService *service = g_server->createService(SERVICE_UUID);

  g_onOffChar = service->createCharacteristic(
    CHAR_ONOFF_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  g_onOffChar->addDescriptor(new BLE2902());

  g_presetChar = service->createCharacteristic(
    CHAR_PRESET_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  g_presetChar->addDescriptor(new BLE2902());

  g_whiteChar = service->createCharacteristic(
    CHAR_WHITE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  g_whiteChar->addDescriptor(new BLE2902());
  g_whiteChar->setCallbacks(new PercentWriteCallback(&StateManager::setWhiteBrightnessPercent, &StateManager::getWhitePercent));

  g_redChar = service->createCharacteristic(
    CHAR_RED_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  g_redChar->addDescriptor(new BLE2902());
  g_redChar->setCallbacks(new PercentWriteCallback(&StateManager::setRedPercent, &StateManager::getRedPercent));

  g_greenChar = service->createCharacteristic(
    CHAR_GREEN_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  g_greenChar->addDescriptor(new BLE2902());
  g_greenChar->setCallbacks(new PercentWriteCallback(&StateManager::setGreenPercent, &StateManager::getGreenPercent));

  g_blueChar = service->createCharacteristic(
    CHAR_BLUE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  g_blueChar->addDescriptor(new BLE2902());
  g_blueChar->setCallbacks(new PercentWriteCallback(&StateManager::setBluePercent, &StateManager::getBluePercent));

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  refreshAll();
}

void BleController::refreshAll() {
  if (!g_stateManager) return;
  writeUint8AndNotify(g_onOffChar, g_stateManager->isOn() ? 1 : 0);
  writeUint8AndNotify(g_presetChar, g_stateManager->getPresetIndex());
  writeUint8AndNotify(g_whiteChar, g_stateManager->getWhitePercent());
  writeUint8AndNotify(g_redChar, g_stateManager->getRedPercent());
  writeUint8AndNotify(g_greenChar, g_stateManager->getGreenPercent());
  writeUint8AndNotify(g_blueChar, g_stateManager->getBluePercent());
}
