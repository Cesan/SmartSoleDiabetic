#include <nvs_flash.h>
#include <esp_pm.h>
#include <esp_private/esp_clk.h>
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_host.h"
#include "sensors.h"

static const char *TAG = "ai-sole";

void app_main() {
    ESP_LOGI(TAG, "Starting...");

//    esp_pm_config_t pm_config = {
//        .max_freq_mhz = 80,
//        .min_freq_mhz = 40,
//        .light_sleep_enable = false
//    };

//    int res = esp_pm_configure(&pm_config);
//    assert(res == 0);

//    vTaskDelay(pdMS_TO_TICKS(10));

//    int freq = esp_clk_cpu_freq();
//
//    ESP_LOGI(TAG, "Current cpu frequency is: %d", freq);

    esp_err_t res = sensors_i2c_init();
    if (res != 0) {
        ESP_LOGE(TAG, "i2c init returned error %d", res);
        return;
    } else {
        ESP_LOGI(TAG, "i2c init successful");
    }

    sensors_init_all();

    res = nvs_flash_init();
    if (res != 0) {
        ESP_LOGE(TAG, "flash init returned error %d", res);
        return;
    } else {
        ESP_LOGI(TAG, "flash init successful");
    }

    sensors_load_data();

    ESP_LOGI(TAG, "loaded sensors data from flash");

    res = nimble_port_init();
    if (res != 0) {
        ESP_LOGE(TAG, "nimble port init returned error %d", res);
        return;
    } else {
        ESP_LOGI(TAG, "nimble port init successful");
    }

    res = ble_host_init();
    if (res != 0) {
        ESP_LOGE(TAG, "gatt server init returned error %d", res);
        return;
    } else {
        ESP_LOGI(TAG, "gatt server init successful");
    }

    res = ble_host_set_device_name();
    assert(res == 0);

    ESP_LOGI(TAG, "ble gap device name set successful");

    ble_host_start();
}