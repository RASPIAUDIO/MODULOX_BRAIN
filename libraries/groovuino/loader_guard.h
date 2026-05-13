#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#ifndef BUT3
#define BUT3 GPIO_NUM_38
#endif

#ifndef BUTENCO
#define BUTENCO GPIO_NUM_39
#endif

static const char *LOADER_GUARD_PREFS_NAMESPACE = "modulox";
static const char *LOADER_GUARD_STAY_ONCE_KEY = "stay_once";
static const uint32_t LOADER_GUARD_HOLD_MS = 4000;
static TaskHandle_t loaderGuardTaskHandle = NULL;

static bool loaderGuardSetBootPartition(const char *label) {
  const esp_partition_t *partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_ANY,
    label
  );
  if (!partition) {
    Serial.printf("[loader_guard] partition %s not found\n", label);
    return false;
  }

  const esp_err_t err = esp_ota_set_boot_partition(partition);
  if (err != ESP_OK) {
    Serial.printf("[loader_guard] esp_ota_set_boot_partition(%s) failed: 0x%x\n",
                  label,
                  (unsigned)err);
    return false;
  }

  Serial.printf("[loader_guard] default boot set to %s\n", label);
  return true;
}

static bool loaderGuardMarkStayLoaderOnce() {
  Preferences prefs;
  if (!prefs.begin(LOADER_GUARD_PREFS_NAMESPACE, false)) {
    Serial.println("[loader_guard] NVS open failed");
    return false;
  }

  const bool ok = prefs.putBool(LOADER_GUARD_STAY_ONCE_KEY, true) == 1;
  prefs.end();
  if (!ok) Serial.println("[loader_guard] stay_once write failed");
  return ok;
}

static void loaderGuardEnsureLoaderDefaultBoot() {
  loaderGuardSetBootPartition("app0");
}

static bool loaderGuardReturnButtonPressed() {
  return digitalRead(BUT3) == HIGH || digitalRead(BUTENCO) == HIGH;
}

static void loaderGuardTask(void *parameter) {
  (void)parameter;

  pinMode(BUT3, INPUT_PULLDOWN);
  pinMode(BUTENCO, INPUT_PULLDOWN);
  vTaskDelay(pdMS_TO_TICKS(700));

  Serial.printf("[loader_guard] task ready BUT3=%d BUTENCO=%d\n",
                digitalRead(BUT3),
                digitalRead(BUTENCO));

  uint32_t holdStartMs = 0;
  bool armed = false;

  while (true) {
    const bool pressed = loaderGuardReturnButtonPressed();

    if (pressed) {
      if (holdStartMs == 0) {
        holdStartMs = millis();
        Serial.println("[loader_guard] return hold detected");
      } else if (!armed && millis() - holdStartMs >= LOADER_GUARD_HOLD_MS) {
        Serial.println("[loader_guard] return hold confirmed, release to restart");
        loaderGuardMarkStayLoaderOnce();
        armed = loaderGuardSetBootPartition("app0");
      }
    } else {
      if (armed) {
        Serial.println("[loader_guard] restarting to loader");
        Serial.flush();
        delay(250);
        esp_restart();
      }
      holdStartMs = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void startLoaderGuard() {
  loaderGuardEnsureLoaderDefaultBoot();
  if (loaderGuardTaskHandle) return;

  xTaskCreatePinnedToCore(
    loaderGuardTask,
    "LoaderGuard",
    4096,
    NULL,
    10,
    &loaderGuardTaskHandle,
    0
  );
}
