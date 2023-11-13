#include <sys/cdefs.h>

#ifndef SOLE_SENSORS_H
#define SOLE_SENSORS_H

#define MAX_SENSORS 31

#define SENSOR_METADATA_MAX_ADDRESS (sizeof(SENSOR_DATA_FLAGS) + (30 * sizeof(sensor_metadata_t)))

extern uint8_t sensorAddress[];

typedef enum __attribute__((packed)) {
    SENSOR_DATA_STORED = 0x01,
    SENSOR_DATA_OVERFLOWED = 0x02
} SENSOR_DATA_FLAGS;

typedef struct __attribute__((packed)) sensor_data {
    uint32_t counter: 24;
    uint32_t data_flag: 8;
    uint32_t time;
    uint8_t sensor_values[31];
} sensor_data_t;

/**
 * Internally initializes the i2c driver for further usage
 * @return The esp error code with the state
 */
esp_err_t sensors_i2c_init();

/**
 * Inits the max31725 sensor for continuous conversion and starts it
 * @param address - The 8 bit address of the device (gets bit-shifted internally)
 */
int sensor_init(uint8_t address);

/**
 * Inits all available sensors in the sole
 */
void sensors_init_all();

/**
 * Loads the saves sensors data from flash.\n
 * This includes: data_counter and current position/offset in flash. Only loaded if data was already stored
 */
void sensors_load_data();

/**
 * Clears all sensor data stored in flash.\n
 * This includes: data_counter and current position/offset in nvs and the complete nvs_ext partition
 */
void sensors_clear_data();

/**
 * Starts the sensor measurement task, only if the task is not running already
 */
void sensors_start_measurement_task();

/**
 * Stops the sensor measurement task
 */
void sensors_stop_measurement_task();

/**
 * Starts the data playback task, only if the task is not running already
 */
void sensors_start_data_play_task();

/**
 * Stops the data playback task
 */
void sensors_stop_data_play_task();

/**
 * Notifies the device of the current count of data stored in the flash
 */
void sensors_notify_data_count();

#endif //SOLE_SENSORS_H
