#include "Buzzer.h"
#include "Config.h"

void Buzzer::begin() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void Buzzer::beep() {
  digitalWrite(BUZZER_PIN, HIGH);
  sounding = true;
  startedAtMs = millis();
}

void Buzzer::update() {
  if (sounding && (millis() - startedAtMs) >= BUZZER_BEEP_MS) {
    digitalWrite(BUZZER_PIN, LOW);
    sounding = false;
  }
}
