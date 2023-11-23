#include <sys/time.h>
#include <host/util/util.h>
#include "nimble/nimble_port_freertos.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sensors.h"
#include "ble_host.h"

//static const ble_uuid128_t service_uuid =
//    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
//
//static const ble_uuid128_t rx_uuid =
//    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
//
//static const ble_uuid128_t tx_uuid =
//    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

const char *TAG = "ble_host";

static const char *device_name = "ai_sole";

static uint8_t addr_type;
static uint16_t connection_handle;

uint8_t sensor_handle_val[64];
uint8_t sensor_handle_val_length;
uint16_t sensor_handle;

uint8_t rx_handle_buf[64];
uint16_t rx_handle;

static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static void ble_start_advertising();

static void close_connection();

void print_address(const void *address) {
    const uint8_t *u8_address = address;

    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x",
             u8_address[5], u8_address[4], u8_address[3], u8_address[2], u8_address[1], u8_address[0]);
}

int mtu_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t mtu, void *arg) {
    ESP_LOGI(TAG, "Exchanged mtu %d with error code %d", mtu, error->status);

    return 0;
}

int gap_event_cb(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    int res;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "connection %s; status=%d ",
                     event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);

            // Print connection information if connected
            if (event->connect.status == 0) {
                connection_handle = event->connect.conn_handle;
                res = ble_gap_conn_find(event->connect.conn_handle, &desc);

                ble_gattc_exchange_mtu(event->connect.conn_handle, mtu_cb, NULL);

                ESP_LOGI(TAG, "connection uses mtu: %d", ble_att_mtu(event->connect.conn_handle));
                assert(res == 0);

                //TODO: print connection information
            }

            // Restart advertisement if not connected anymore
            if (event->connect.status != 0) {
                ble_start_advertising();
                connection_handle = 0;

                close_connection();
            }

            return 0;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "disconnect, reason = %d", event->disconnect.reason);
            connection_handle = 0;

            close_connection();

            // Restart advertisement if disconnected
            ble_start_advertising();
            return 0;
        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "connection updated, status = %d", event->conn_update.status);

            res = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            assert(res == 0);

            return 0;
        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            ESP_LOGI(TAG, "connection update request");

            res = ble_gap_conn_find(event->conn_update_req.conn_handle, &desc);
            assert(res == 0);

            const struct ble_gap_upd_params *peer = event->conn_update_req.peer_params;

            ESP_LOGI(TAG, "Peer params, itvl: %d - %d, con_evt_len: %d - %d, timeout: %d", peer->itvl_min,
                     peer->itvl_max, peer->min_ce_len, peer->max_ce_len, peer->supervision_timeout);

            struct ble_gap_upd_params *self = event->conn_update_req.self_params;

            ESP_LOGI(TAG, "Self params, itvl: %d - %d, con_evt_len: %d - %d, timeout: %d", self->itvl_min,
                     self->itvl_max, self->min_ce_len, self->max_ce_len, self->supervision_timeout);

            return 0;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "advertise complete, reason = %d", event->adv_complete.reason);

            // Restart advertisement if finished
            ble_start_advertising();

            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG, "encryption changed, status = %d", event->enc_change.status);

            res = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(res == 0);

            return 0;
        case BLE_GAP_EVENT_NOTIFY_TX:
            ESP_LOGI(TAG, "notify_tx event; conn_handle=%d attr_handle=%d "
                          "status=%d is_indication=%d",
                     event->notify_tx.conn_handle,
                     event->notify_tx.attr_handle,
                     event->notify_tx.status,
                     event->notify_tx.indication);

            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                     event->subscribe.conn_handle,
                     event->subscribe.attr_handle,
                     event->subscribe.reason,
                     event->subscribe.prev_notify,
                     event->subscribe.cur_notify,
                     event->subscribe.prev_indicate,
                     event->subscribe.cur_indicate);

            if (event->subscribe.attr_handle == sensor_handle && event->subscribe.cur_notify == 1) {
                // Notify device of current saved data count on subscription
                sensors_notify_data_count();

                struct ble_gap_upd_params params = {
                    .itvl_min = BLE_GAP_CONN_ITVL_MS(500),
                    .itvl_max = BLE_GAP_CONN_ITVL_MS(1500),
                    .latency = 0,
                    .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(24000)
                };

                res = ble_gap_update_params(event->subscribe.conn_handle, &params);
                if (res != ESP_OK) {
                    ESP_LOGW(TAG, "GAP update params error with code: %d", res);
                }
            }

            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                     event->mtu.conn_handle,
                     event->mtu.channel_id,
                     event->mtu.value);

//            res = ble_att_set_preferred_mtu(event->mtu.value);
//            assert(res == 0);

            ESP_LOGI(TAG, "connection uses mtu: %d", ble_att_mtu(event->mtu.conn_handle));

            return 0;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            res = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(res == 0);

            // Delete peer if pairing needs to be repeated
            ble_store_util_delete_peer(&desc.peer_id_addr);

            return BLE_GAP_REPEAT_PAIRING_RETRY;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            return 0;
        case BLE_GAP_EVENT_TRANSMIT_POWER:
            break;
        case BLE_GAP_EVENT_PATHLOSS_THRESHOLD:
            break;
    }
    return 0;
}

/**
 * Starts advertising and sets the needed field values for correct detection e.g. service uuid and name
 */
static void ble_start_advertising() {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int res;

    memset(&fields, 0, sizeof(fields));

    /*
     * Discoverable (general)
     * No support for BR/EDR (BLE only mode)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Sets tx power level to auto mode
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    // Sets the advertised uuid to the service uuid for the sole-ble server so the app can connect
    fields.uuids128 = (ble_uuid128_t[]) {
        service_uuid
    };
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    res = ble_gap_adv_set_fields(&fields);
    if (res != 0) {
        ESP_LOGE(TAG, "Error while setting advertisement data, response: %d", res);
        return;
    }

    // Advertisement response data to allow more data to be sent
    memset(&rsp_fields, 0, sizeof(rsp_fields));

    // Advertised name for the app to display
    rsp_fields.name = (uint8_t *) device_name;
    rsp_fields.name_len = strlen(device_name);
    rsp_fields.name_is_complete = 1;

    res = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (res != 0) {
        ESP_LOGE(TAG, "Error while setting adv response data, response: %d", res);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));

    /*
     * Allow undirected connection
     * Make general discoverable
     */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /**
     * Set advertising interval in a range from 500-1000ms
     */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(1000);

    res = ble_gap_adv_start(addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (res != 0) {
        ESP_LOGE(TAG, "Error starting advertisement, response: %d", res);
        return;
    }
}

void ble_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE reset, reason: %d", reason);
}

void ble_on_sync() {
    int res;

    // Ensure generation of the device address in non-random mode
    res = ble_hs_util_ensure_addr(0);
    assert(res == 0);

    // Gets address type in public address mode
    res = ble_hs_id_infer_auto(0, &addr_type);
    assert(res == 0);

    // Copies the devices address into addr_val
    uint8_t addr_val[6] = {0};
    res = ble_hs_id_copy_addr(addr_type, addr_val, NULL);
    assert(res == 0);

    ESP_LOGI(TAG, "BLE device address:");
    print_address(addr_val);

    ble_start_advertising();
}

void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started");

    nimble_port_run();

    nimble_port_freertos_deinit();
}

void sole_receive_handler(const uint8_t *data, uint16_t len) {
    if (len == 0)
        return;

    char *end;

    time_t cur_time = strtoll((char *) &data[1], &end, 10);

    struct timeval time;

    time.tv_sec = cur_time;
    time.tv_usec = 0;

    settimeofday(&time, NULL);

    switch (data[0]) {
        case 'R':
            ESP_LOGD(TAG, "Received START command");
            sensors_stop_data_play_task();
            sensors_start_measurement_task();
            break;
        case 'S':
            ESP_LOGD(TAG, "Received STOP command");
            sensors_stop_measurement_task();
            break;
        case 'P':
            ESP_LOGD(TAG, "Received PLAY command");
            sensors_stop_measurement_task();
            sensors_start_data_play_task();
            break;
        case 'H':
            ESP_LOGD(TAG, "Received HALT command");
            sensors_stop_data_play_task();
            break;
        case 'C':
            ESP_LOGD(TAG, "Received CLEAR command");
            sensors_clear_data();
            break;
        default:
            ESP_LOGD(TAG, "Command not recognized");
            break;
    }
}

int mbuf_to_flat(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                 void *dst, uint16_t *len) {
    uint16_t om_len;
    int res;

    om_len = OS_MBUF_PKTLEN(om);

    ESP_LOGI(TAG, "Data write with len: %d", om_len);

    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint16_t data_length;

    res = ble_hs_mbuf_to_flat(om, dst, max_len, &data_length);
    if (res != 0) {
        return res;
    }

    ESP_LOG_BUFFER_HEXDUMP(TAG, dst, max_len, ESP_LOG_INFO);

    if (len)
        *len = data_length;

    return 0;
}

static int gatt_characteristic_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                         struct ble_gatt_access_ctxt *context, void *arg) {

    const ble_uuid_t *uuid;
    int res;

    ESP_LOGI(TAG, "Server access with op: %d", context->op);

    switch (context->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
                ESP_LOGI(TAG, "Characteristic read, conn_handle = %d attr_handle = %d", conn_handle, attr_handle);
            else
                ESP_LOGI(TAG, "Characteristic read by NimBLE stack, attr_handle = %d", attr_handle);

            uuid = context->chr->uuid;
            // Read event for sensor value characteristic (tx handle)
            if (attr_handle == sensor_handle) {
                // Fills the events memory buffer with sensor data
                res = os_mbuf_append(context->om, sensor_handle_val, sensor_handle_val_length);

                return res == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            if (attr_handle == rx_handle) {
                return 0;
            }

            goto unknown;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
                ESP_LOGI(TAG, "Characteristic write, conn_handle = %d attr_handle = %d", conn_handle, attr_handle);
            else
                ESP_LOGI(TAG, "Characteristic write by NimBLE stack, attr_handle = %d", attr_handle);

            uuid = context->chr->uuid;
            // Write event for rx characteristic
            if (attr_handle == rx_handle) {
                // Resets the content of the rx buffer and fills it with the value from the ble stack
                memset(rx_handle_buf, 0, sizeof(rx_handle_buf));

                uint16_t len;

                res = mbuf_to_flat(context->om, 1, sizeof(rx_handle_buf),
                                   rx_handle_buf, &len);

                sole_receive_handler(rx_handle_buf, len);

//                ble_gatts_chr_updated(rx_handle);
//
//                ESP_LOGI(TAG, "Notification/Indication scheduled for all subscribed peers. Write response: %d", res);
                return res;
            }

            if (attr_handle == sensor_handle) {
                return 0;
            }

            goto unknown;
        case BLE_GATT_ACCESS_OP_READ_DSC:
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
                ESP_LOGI(TAG, "Descriptor read, conn_handle = %d attr_handle = %d", conn_handle, attr_handle);
            else
                ESP_LOGI(TAG, "Descriptor read by NimBLE stack, attr_handle = %d", attr_handle);

            goto unknown;
        case BLE_GATT_ACCESS_OP_WRITE_DSC:
            if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
                ESP_LOGI(TAG, "Descriptor write, conn_handle = %d attr_handle = %d", conn_handle, attr_handle);
            else
                ESP_LOGI(TAG, "Descriptor write by NimBLE stack, attr_handle = %d", attr_handle);

            goto unknown;
        default:
            goto unknown;
    }

    unknown:

    ESP_LOGW(TAG, "Unknown or invalid gatt characteristic access event");

    return BLE_ATT_ERR_UNLIKELY;
}

// 3000eacd-4b54-4fd8-a13c-f746103504c3

static const struct ble_gatt_svc_def gatt_srv_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t *) &service_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID128_DECLARE(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5,
                                            0x02, 0x00, 0x40, 0x6e),
                .access_cb = gatt_characteristic_access_cb,
                .val_handle = &rx_handle,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE
            },
            {
                .uuid = BLE_UUID128_DECLARE(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5,
                                            0x03, 0x00, 0x40, 0x6e),
                .access_cb = gatt_characteristic_access_cb,
                .val_handle = &sensor_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY
            },
            {
                0
            }
        }
    },
    {
        0
    }
};

void ble_gatt_service_register_cb(struct ble_gatt_register_ctxt *context, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (context->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG, "Registered service %s with handle=%d\n",
                     ble_uuid_to_str(context->svc.svc_def->uuid, buf),
                     context->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG, "Registering characteristic %s with "
                          "def_handle=%d val_handle=%d\n",
                     ble_uuid_to_str(context->chr.chr_def->uuid, buf),
                     context->chr.def_handle,
                     context->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD(TAG, "Registering descriptor %s with handle=%d\n",
                     ble_uuid_to_str(context->dsc.dsc_def->uuid, buf),
                     context->dsc.handle);
            break;

        default:
            ESP_LOGW(TAG, "Invalid gatt service register event");
            break;
    }
}

int ble_host_set_device_name() {
    return ble_svc_gap_device_name_set(device_name);
}

int ble_host_init() {
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = ble_gatt_service_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int res = ble_gatts_count_cfg(gatt_srv_services);
    if (res != 0)
        return res * -1;

    res = ble_gatts_add_svcs(gatt_srv_services);
    if (res != 0)
        return res;

    return 0;
}

void ble_host_start() {
    nimble_port_freertos_init(nimble_host_task);
}

static void close_connection() {
    sensors_stop_measurement_task();
    sensors_stop_data_play_task();
}