#ifndef AISOLE_BLE_HOST_H
#define AISOLE_BLE_HOST_H

extern uint8_t sensor_handle_val[64];
extern uint8_t sensor_handle_val_length;
extern uint16_t sensor_handle;
extern uint16_t rx_handle;

//void ble_gatt_service_register_cb(struct ble_gatt_register_ctxt *context, void *arg);

/**
 * Sets the ble hosts device name used for internal stack usage
 * @return The error code of the internal gap device set
 */
int ble_host_set_device_name();

/**
 * Initialized the gatt server and adds the required characteristics
 * @return The error code of the internal gatt/gap initialization
 */
int ble_host_init();

/**
 * Starts the nimble port host task
 */
void ble_host_start();

#endif //AISOLE_BLE_HOST_H
