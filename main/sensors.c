#include <sys/cdefs.h>
#include <sys/time.h>
#include <string.h>
#include <esp_bt.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "esp_partition.h"
#include "nvs.h"
#include "ble_host.h"
#include "sensors.h"

#define SDA_IO_NUM 6
#define SCL_IO_NUM 7
#define MASTER_CLK_FREQ 100000

#define DATA_VALUE_INTERVAL 60000
#define PLAY_DATA_INTERVAL 2000

static const char *TAG = "Sensors";

static const char *DATA_COUNT_KEY = "data_count";
static const char *ADDRESS_OFFSET_KEY = "address_offset";

static uint32_t data_counter = 0;
static uint32_t play_counter = 0;
static uint32_t address_offset = 0;

// max31725 sensors i2c addresses in the sole
// sensor u1 is not used
uint8_t sensor_address[MAX_SENSORS] = {
    // u1    u2    u3    u4    u5    u6    u7    u8
    0x92, 0x82, 0x80, 0x94, 0x96, 0x86, 0x84,
    // u9    u10   u11   u12   u13   u14   u15   u16
    0xb4, 0xb6, 0xa6, 0xa4, 0xb0, 0xb2, 0xa2, 0xa0,
    // u17   u18   u19   u20   u21   u22   u23   u24
    0x98, 0x9a, 0x8a, 0x88, 0x9c, 0x9e, 0x8e, 0x8c,
    // u25   u26   u27   u28   u29   u30   u31   u32
    0xbc, 0xbe, 0xae, 0xac, 0xb8, 0xba, 0xaa, 0xa8
};

uint8_t i2c_wbuf[2];
uint8_t i2c_rbuf[2];
uint8_t tx_buf[CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU];

TaskHandle_t sensor_task = NULL;
TaskHandle_t data_play_task = NULL;

_Noreturn static void read_sensor_loop();

static void data_play_loop();

/**
 * @brief Perform a write followed by a read to a device on the I2C bus.
 *        A repeated start signal is used between the `write` and `read`, thus, the bus is
 *        not released until the two transactions are finished.
 *        This function is a wrapper to 'i2c_master_write_read_device'
 *        It shall only be called in I2C master mode.
 *
 * @param device_address I2C device's 7-bit address
 * @param write_buf Bytes to send on the bus
 * @param write_len Length, in bytes, of the write buffer
 * @param read_buf Buffer to store the bytes received on the bus
 * @param read_len Length, in bytes, of the read buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t read_from_device(uint8_t device_address, const uint8_t *write_buf, uint8_t write_len, uint8_t *read_buf,
                           uint8_t read_len) {
    return i2c_master_write_read_device(I2C_NUM_0, device_address,
                                        write_buf, write_len,
                                        read_buf, read_len,
                                        pdMS_TO_TICKS(2));
}

/**
 * @brief Perform a write to a device connected to a particular I2C port.
 *        This function is a wrapper to 'i2c_master_write_to_device'
 *        It shall only be called in I2C master mode.
 *
 * @param device_address I2C device's 7-bit address
 * @param write_buf Bytes to send on the bus
 * @param write_len Length, in bytes, of the write buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Sending command error, slave hasn't ACK the transfer.
 *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 */
esp_err_t write_to_device(uint8_t device_address, const uint8_t *write_buf, uint8_t write_len) {
    return i2c_master_write_to_device(I2C_NUM_0, device_address,
                                      write_buf, write_len,
                                      pdMS_TO_TICKS(2));
}

esp_err_t sensors_i2c_init() {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_IO_NUM,
        .scl_io_num = SCL_IO_NUM,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = MASTER_CLK_FREQ
    };

    i2c_param_config(I2C_NUM_0, &cfg);

    gpio_sleep_set_direction(SDA_IO_NUM, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_sleep_set_pull_mode(SDA_IO_NUM, GPIO_PULLUP_ONLY);

    gpio_sleep_set_direction(SCL_IO_NUM, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_sleep_set_pull_mode(SCL_IO_NUM, GPIO_PULLUP_ONLY);

    return i2c_driver_install(I2C_NUM_0, cfg.mode, 0, 0, 0);
}

/**
 * Inits the max31725 sensor for continuous conversion and starts it
 * @param address - The 8 bit address of the device (gets bit-shifted internally)
 */
int sensor_init(uint8_t address) {
    ESP_LOGD(TAG, "Init at 0x%02x ...", address);

    address >>= 1;

    // Access config
    i2c_wbuf[0] = 0x01;
    // Set to shutdown-mode for usage with one-shot operation
    i2c_wbuf[1] = MAX_31725_SHUTDOWN;

    int res = write_to_device(address, i2c_wbuf, 2);
    if (res != 0)
        return res;

    res = write_to_device(address, i2c_wbuf, 1);
    if (res != 0)
        return res;

    //ESP_LOGV(TAG, "Done init");

    return ESP_OK;
}

void sensors_init_all() {
    int res;

    for (int i = 0; i < MAX_SENSORS; i++) {
        res = sensor_init(sensor_address[i]);

        if (res != ESP_OK)
            ESP_LOGW(TAG, "Failed to initialize sensor %d with error 0x%02x", i, res);
    }
}

/**
 * Reads the sensor at the given address
 * @param address - The 8 bit address of the device (gets bit-shifted internally)
 * @return The read temperature encoded as 7 bit Value, 1 bit decimal point
 */
uint8_t read_sensor(uint8_t address) {
    ESP_LOGV(TAG, "Read at 0x%02x ...", address);

    address >>= 1;

    i2c_wbuf[0] = 0x00;

    int res = read_from_device(address, i2c_wbuf, 1, i2c_rbuf, 2);
    if (res != 0)
        return 0;

    uint8_t raw_temperature = i2c_rbuf[0];
    uint8_t decimals = i2c_rbuf[1];

    uint8_t temperature = raw_temperature << 1;
    if (decimals & 0x80)
        temperature++;

    ESP_LOGV(TAG, "Done reading, temperature: %.1f (%d) - %02x/%u %02x/%u", (float) temperature / 2.0, temperature,
             raw_temperature, raw_temperature, decimals, decimals);

    return temperature;
}

void sensors_load_data() {
    nvs_handle_t handle;

    esp_err_t res = nvs_open("sensor_data", NVS_READWRITE, &handle);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Reading partition with sensor data flags failed, reason %s", esp_err_to_name(res));
        goto end;
    }

    res = nvs_get_u32(handle, DATA_COUNT_KEY, &data_counter);
    if (res == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No sensor metadata is stored in flash, returning");
        goto end;
    } else if (res != ESP_OK) {
        ESP_LOGW(TAG, "Reading nvs key data_count failed, reason %s", esp_err_to_name(res));
        goto end;
    }

    res = nvs_get_u32(handle, ADDRESS_OFFSET_KEY, &address_offset);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Reading nvs key address_offset failed, reason %s", esp_err_to_name(res));
        goto end;
    }

    end:
    nvs_close(handle);
}

void sensors_clear_data() {
    nvs_handle_t handle;

    esp_err_t res = nvs_open("sensor_data", NVS_READWRITE, &handle);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Reading partition with sensor data flags failed, reason %s", esp_err_to_name(res));
        goto end;
    }

    nvs_erase_key(handle, DATA_COUNT_KEY);
    nvs_erase_key(handle, ADDRESS_OFFSET_KEY);

    nvs_commit(handle);

    end:
    nvs_close(handle);

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                                "nvs_ext");

    if (partition)
        esp_partition_erase_range(partition, 0, partition->size);

    data_counter = 0;
    play_counter = 0;
}

void save_sensor_data(sensor_data_t *sensor_data) {
    nvs_handle_t handle;

    esp_err_t res = nvs_open("sensor_data", NVS_READWRITE, &handle);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Reading partition with sensor data flags failed, reason %s", esp_err_to_name(res));
        goto end;
    }

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                                "nvs_ext");
    if (partition == NULL) {
        ESP_LOGW(TAG, "Finding partition nvs_ext failed");
        goto end;
    }

    esp_partition_write(partition, address_offset, &sensor_data->time, sizeof(sensor_data_t) - 4);

    address_offset += sizeof(sensor_data_t) - 4;

    nvs_set_u32(handle, DATA_COUNT_KEY, data_counter);
    nvs_set_u32(handle, ADDRESS_OFFSET_KEY, address_offset);

    nvs_commit(handle);

    end:
    nvs_close(handle);
}

void sensors_start_measurement_task() {
    if (!sensor_task)
        xTaskCreate(read_sensor_loop, "sensor_task", 2048, NULL, 4, &sensor_task);
}

void sensors_stop_measurement_task() {
    if (sensor_task)
        vTaskDelete(sensor_task);

    sensor_task = NULL;
}

void sensors_start_data_play_task() {
    if (!data_play_task)
        xTaskCreate(data_play_loop, "data_play_task", 2048, NULL, 4, &data_play_task);
}

void sensors_stop_data_play_task() {
    if (data_play_task)
        vTaskDelete(data_play_task);

    data_play_task = NULL;
}

void sensors_notify_data_count() {
    memcpy((void *) sensor_handle_val, (void *) &data_counter, sizeof(uint32_t));

    sensor_handle_val[3] = 22;
    sensor_handle_val_length = 39;

    ble_gatts_chr_updated(sensor_handle);
}

_Noreturn static void read_sensor_loop() {
    while (1) {
        data_counter++;

        time_t current_time = time(NULL);

        sensor_data_t data;

        data.counter = data_counter;
        data.data_flag = 11;
        data.time = current_time;

        i2c_wbuf[0] = 0x01;
        i2c_wbuf[1] = MAX_31725_ONE_SHOT | MAX_31725_SHUTDOWN;

        for (int i = 0; i < MAX_SENSORS; i++) {
            write_to_device(sensor_address[i] >> 1, i2c_wbuf, 2);
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        read_from_device(sensor_address[MAX_SENSORS - 1] >> 1, i2c_wbuf, 1, i2c_rbuf, 1);

        if ((i2c_rbuf[0] & MAX_31725_ONE_SHOT) != 0) {
            ESP_LOGW(TAG, "Sensors temperature not ready after 50ms");
        }

        for (int i = 0; i < MAX_SENSORS; ++i) {
            data.sensor_values[i] = read_sensor(sensor_address[i]);
        }

        //ESP_LOGD(TAG, "* #%u time:%llu temperatures:%.1f %.1f %.1f ...\n", 0, current_time, (float) tx_buf[8] / 2.0,
        //         (float) tx_buf[9] / 2.0, (float) tx_buf[10] / 2.0);

        ESP_LOGD(TAG, "* #%u time:%llu temperatures:%.1f %.1f %.1f ...\n", 0, current_time,
                 (float) data.sensor_values[0] / 2.0,
                 (float) data.sensor_values[1] / 2.0, (float) data.sensor_values[2] / 2.0);

        memcpy((void *) sensor_handle_val, (void *) &data, sizeof(sensor_data_t));

        //memcpy((void *) sensor_handle_val, (void*) tx_buf, 39);
        sensor_handle_val_length = sizeof(sensor_data_t); // 4 + 4 + 31

        save_sensor_data(&data);

        ble_gatts_chr_updated(sensor_handle);

        vTaskDelay(pdMS_TO_TICKS(DATA_VALUE_INTERVAL));
    }
}

static void data_play_loop() {
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS,
                                                                "nvs_ext");

    if (partition == NULL) {
        ESP_LOGW(TAG, "Finding partition nvs_ext failed");
        goto end;
    }

    ESP_LOGD(TAG, "Starting data playing, current play counter: %lu and data_counter: %lu", play_counter, data_counter);

    while (1) {
        if (play_counter > data_counter || data_counter == 0) {
            goto end;
        } else if (play_counter == 0) {
            play_counter = 1;
        }

        esp_err_t res = esp_partition_read(partition, (play_counter - 1) * (sizeof(sensor_data_t) - 4), tx_buf,
                                           sizeof(sensor_data_t) - 4);
        if (res != ESP_OK) {
            ESP_LOGW(TAG, "Reading partition with sensor data failed, reason %s", esp_err_to_name(res));
            goto end;
        }

        memcpy((void *) sensor_handle_val + 4, (void *) tx_buf, sizeof(sensor_data_t) - 4);
        memcpy((void *) sensor_handle_val, (void *) &play_counter, sizeof(int));

        sensor_handle_val[3] = 12;

        sensor_handle_val_length = sizeof(sensor_data_t);
        ble_gatts_chr_updated(sensor_handle);

        play_counter++;

        vTaskDelay(pdMS_TO_TICKS(PLAY_DATA_INTERVAL));
    }

    end:
    ESP_LOGD(TAG, "Finished playing data");
    play_counter = 0;
    data_play_task = NULL;
    vTaskDelete(NULL);
}