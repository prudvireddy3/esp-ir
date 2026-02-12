#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* kTag = "esp-ir";

extern "C" void app_main(void) {
  ESP_LOGI(kTag, "ESP-IR firmware scaffold boot");

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
