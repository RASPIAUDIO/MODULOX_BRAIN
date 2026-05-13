#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

#ifndef BUTLEFT
#define BUTLEFT GPIO_NUM_0
#endif

TaskHandle_t ReturnLoaderTaskHandle = NULL;

bool selectLoaderForNextBoot() {
  const esp_partition_t *loaderPartition = esp_partition_find_first(
    ESP_PARTITION_TYPE_APP,
    ESP_PARTITION_SUBTYPE_APP_OTA_0,
    "app0"
  );
  if (!loaderPartition) {
    Serial.println("[loader] app0 partition not found");
    return false;
  }

  const esp_err_t err = esp_ota_set_boot_partition(loaderPartition);
  if (err != ESP_OK) {
    Serial.printf("[loader] esp_ota_set_boot_partition failed: 0x%x\n", (unsigned)err);
    return false;
  }

  Serial.println("[loader] app0 selected");
  return true;
}

void returnToLoaderTask(void *parameter) {
  (void)parameter;

  pinMode(BUTLEFT, INPUT_PULLUP);
  vTaskDelay(pdMS_TO_TICKS(700));

  const bool menuIdle = digitalRead(BUTLEFT);
  uint32_t menuStartMs = 0;
  bool armed = false;

  Serial.printf("[loader] task idle menu=%d\n", menuIdle ? 1 : 0);

  while (true) {
    const bool menuPressed = digitalRead(BUTLEFT) != menuIdle;

    if (menuPressed) {
      if (menuStartMs == 0) {
        menuStartMs = millis();
        Serial.println("[loader] menu hold detected");
      } else if (!armed && millis() - menuStartMs >= 4000) {
        Serial.println("[loader] menu hold confirmed, release to restart");
        armed = selectLoaderForNextBoot();
      }
    } else {
      if (armed) {
        Serial.println("[loader] returning to app0");
        Serial.flush();
        delay(250);
        esp_restart();
      }
      menuStartMs = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void startReturnToLoaderTask() {
  if (ReturnLoaderTaskHandle) return;
  xTaskCreatePinnedToCore(
    returnToLoaderTask,
    "ReturnLoader",
    4096,
    NULL,
    10,
    &ReturnLoaderTaskHandle,
    0
  );
}
