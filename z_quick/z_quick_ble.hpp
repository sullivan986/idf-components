// multiple character in one service

#ifndef _Z_QUICK_BLE_HPP_
#define _Z_QUICK_BLE_HPP_

#include "array"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "unordered_map"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static const char *TAG = "Z_QUICK_BLE";

enum
{
    IDX_SVC,

    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    // IDX_CHAR_B,
    // IDX_CHAR_VAL_B,

    // IDX_CHAR_C,
    // IDX_CHAR_VAL_C,

    HRS_IDX_NB,
};

// 固定的UUID
static uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// 固定的一些申明值
[[maybe_unused]] static uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
[[maybe_unused]] static uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
[[maybe_unused]] static uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;
[[maybe_unused]] static uint8_t char_prop_read_write_notify =
    ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

// ---var---
static uint8_t adv_config_done;                // adv完成检测 和他的FLAG
static uint8_t ADV_CONFIG_FLAG{(1 << 0)};      //
static uint8_t SCAN_RSP_CONFIG_FLAG{(1 << 1)}; //

static uint16_t handle_table[HRS_IDX_NB]; //
// 0x00ff service_uuid
static uint8_t service_uuid[16] = {
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

static uint8_t SVC_INST_ID{0};                    //
static uint16_t GATTS_DEMO_CHAR_VAL_LEN_MAX{500}; //
static uint16_t PREPARE_BUF_MAX_SIZE{1024};       //
// constexpr static auto PROFILE_APP_IDX{0}; 只有一个  PROFILE
static uint16_t ESP_APP_ID{0x55};
static uint16_t GATTS_SERVICE_UUID{0x00ff}; // service 的 uuid
static uint8_t GATTS_CHAR_UUID_A{0};

static std::array<uint8_t, 4> char_a_buf;
static std::array<uint8_t, 2> char_a_cfg_buf;

// struct
typedef struct
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;
static prepare_type_env_t prepare_write_env;

// some useless declaration
static struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
} profile_tab;

// gatt db 表

static esp_gatts_attr_db_t gatt_db[HRS_IDX_NB]{
    // 服务申明
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP},
                 {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t),
                  sizeof(GATTS_SERVICE_UUID), (uint8_t *)&GATTS_SERVICE_UUID}},

    // 宣称CHAR_A的权限
    [IDX_CHAR_A] = {{ESP_GATT_AUTO_RSP},
                    {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, 1, 1,
                     (uint8_t *)&char_prop_read_write_notify}},

    // CHAR_A的 var
    [IDX_CHAR_VAL_A] = {{ESP_GATT_AUTO_RSP},
                        {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_A, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                         GATTS_DEMO_CHAR_VAL_LEN_MAX, char_a_buf.size(), (uint8_t *)char_a_buf.data()}},

    // CHAR_A的 Configuration
    [IDX_CHAR_CFG_A] = {{ESP_GATT_AUTO_RSP},
                        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), char_a_cfg_buf.size(),
                         (uint8_t *)char_a_cfg_buf.data()}},

};

// 广播参数
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = {},
    .peer_addr_type = {},
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, // test_manufacturer,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
        adv_config_done &= (~ADV_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
    }
    break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: {
        adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
    }
    break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
    }
    break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: { // advertising start complete event to indicate advertising start
                                               // successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "advertising start failed");
        }
        else
        {
            ESP_LOGI(TAG, "advertising start successfully");
        }
    }
    break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
    }
    break;
    case ESP_GAP_BLE_KEY_EVT: {
    }
    break;
    case ESP_GAP_BLE_SEC_REQ_EVT: {
    }
    break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: {
    }
    break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
    }
    break;
    case ESP_GAP_BLE_OOB_REQ_EVT: {
    }
    break;
    case ESP_GAP_BLE_LOCAL_IR_EVT: {
    }
    break;
    case ESP_GAP_BLE_LOCAL_ER_EVT: {
    }
    break;
    case ESP_GAP_BLE_NC_REQ_EVT: {
    }
    break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Advertising stop failed");
        }
        else
        {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;
    }
    break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT: {
    }
    break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: {
        ESP_LOGI(TAG,
                 "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, "
                 "timeout = %d",
                 param->update_conn_params.status, param->update_conn_params.min_int, param->update_conn_params.max_int,
                 param->update_conn_params.conn_int, param->update_conn_params.latency,
                 param->update_conn_params.timeout);
    }
    break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_UPDATE_DUPLICATE_EXCEPTIONAL_LIST_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_CHANNELS_EVT: {
    }
    break;
    case ESP_GAP_BLE_READ_PHY_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_PREFERRED_DEFAULT_PHY_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_PREFERRED_PHY_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_SET_REMOVE_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_SET_CLEAR_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_ADD_DEV_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_REMOVE_DEV_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_CLEAR_DEV_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PREFER_EXT_CONN_PARAMS_SET_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT: {
    }
    break;
    case ESP_GAP_BLE_SCAN_TIMEOUT_EVT: {
    }
    break;
    case ESP_GAP_BLE_ADV_TERMINATED_EVT: {
    }
    break;
    case ESP_GAP_BLE_SCAN_REQ_RECEIVED_EVT: {
    }
    break;
    case ESP_GAP_BLE_CHANNEL_SELECT_ALGORITHM_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT: {
    }
    break;
    case ESP_GAP_BLE_SC_OOB_REQ_EVT: {
    }
    break;
    case ESP_GAP_BLE_SC_CR_LOC_OOB_EVT: {
    }
    break;
    case ESP_GAP_BLE_GET_DEV_NAME_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_RECV_ENABLE_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TRANS_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SET_INFO_TRANS_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_SET_PAST_PARAMS_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TRANS_RECV_EVT: {
    }
    break;
    case ESP_GAP_BLE_DTM_TEST_UPDATE_EVT: {
    }
    break;
    case ESP_GAP_BLE_ADV_CLEAR_COMPLETE_EVT: {
    }
    break;
    case ESP_GAP_BLE_EVT_MAX: {
    }
    break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    // start event
    switch (event)
    {
    case ESP_GATTS_REG_EVT: {
        /* If event is register event, store the gatts_if for each profile */
        if (param->reg.status == ESP_GATT_OK)
        {
            profile_tab.gatts_if = gatts_if;
        }
        else
        {
            ESP_LOGE(TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
        if (!(gatts_if == ESP_GATT_IF_NONE || gatts_if == profile_tab.gatts_if))
        {
            return;
        }

        // config device name
        esp_err_t set_dev_name_ret = esp_bt_dev_set_device_name("ESP32_S3_SP");
        if (set_dev_name_ret)
        {
            ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        // config adv data
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret)
        {
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
        adv_config_done |= ADV_CONFIG_FLAG;
        // config scan response data
        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret)
        {
            ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
        }
        adv_config_done |= SCAN_RSP_CONFIG_FLAG;

        esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
        if (create_attr_ret)
        {
            ESP_LOGE(TAG, "create attr table failed, error code = %x", create_attr_ret);
        }
    }
    break;
    case ESP_GATTS_READ_EVT: {
        ESP_LOGI(TAG, "BLE_READ");
        break;
    }
    break;
    case ESP_GATTS_WRITE_EVT: {
        if (!param->write.is_prep)
        {
            // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle,
                     param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            // 对 char a 的 cfg 进行读取操作
            if (handle_table[IDX_CHAR_CFG_A] == param->write.handle && param->write.len == 2)
            {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                // 1: 发notify 2: 发indicate 0: all disable
                if (descr_value == 0x0001)
                {
                    ESP_LOGI(TAG, "notify enable");
                    uint8_t notify_data[15];
                    for (int i = 0; i < sizeof(notify_data); ++i)
                    {
                        notify_data[i] = i % 0xff;
                    }
                    // the size of notify_data[] need less than MTU size
                    esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, handle_table[IDX_CHAR_VAL_A],
                                                sizeof(notify_data), notify_data, false);
                }
                else if (descr_value == 0x0002)
                {
                    ESP_LOGI(TAG, "indicate enable");
                    uint8_t indicate_data[15];
                    for (int i = 0; i < sizeof(indicate_data); ++i)
                    {
                        indicate_data[i] = i % 0xff;
                    }
                    esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, handle_table[IDX_CHAR_VAL_A],
                                                sizeof(indicate_data), indicate_data, true);
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(TAG, "notify/indicate disable ");
                }
                else
                {
                    ESP_LOGE(TAG, "unknown descr value");
                    esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                }
            }
            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp)
            {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
        }
        else
        {
            // --------------- 准备写值之前
            /* handle prepare write */
            ESP_LOGI(TAG, "prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
            esp_gatt_status_t status = ESP_GATT_OK;
            if (param->write.offset > PREPARE_BUF_MAX_SIZE)
            {
                status = ESP_GATT_INVALID_OFFSET;
            }
            else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE)
            {
                status = ESP_GATT_INVALID_ATTR_LEN;
            }
            if (status == ESP_GATT_OK && prepare_write_env.prepare_buf == NULL)
            {
                prepare_write_env.prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
                prepare_write_env.prepare_len = 0;
                if (prepare_write_env.prepare_buf == NULL)
                {
                    ESP_LOGE(TAG, "%s, Gatt_server prep no mem", __func__);
                    status = ESP_GATT_NO_RESOURCES;
                }
            }

            /*send response when param->write.need_rsp is true */
            /* 如果对方需要rep */
            if (param->write.need_rsp)
            {
                esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
                if (gatt_rsp != NULL)
                {
                    gatt_rsp->attr_value.len = param->write.len;
                    gatt_rsp->attr_value.handle = param->write.handle;
                    gatt_rsp->attr_value.offset = param->write.offset;
                    gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                    memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
                    esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                                                         param->write.trans_id, status, gatt_rsp);
                    if (response_err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Send response error");
                    }
                    free(gatt_rsp);
                }
                else
                {
                    ESP_LOGE(TAG, "%s, malloc failed", __func__);
                    status = ESP_GATT_NO_RESOURCES;
                }
            }
            if (status != ESP_GATT_OK)
            {
                return;
            }
            memcpy(prepare_write_env.prepare_buf + param->write.offset, param->write.value, param->write.len);
            prepare_write_env.prepare_len += param->write.len;
        }
    }
    break;
    case ESP_GATTS_EXEC_WRITE_EVT: { // the length of gattc prepare write data must be less than
                                     // GATTS_DEMO_CHAR_VAL_LEN_MAX.
        // 开始写
        ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");
        if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC && prepare_write_env.prepare_buf)
        {
            esp_log_buffer_hex(TAG, prepare_write_env.prepare_buf, prepare_write_env.prepare_len);
        }
        else
        {
            ESP_LOGI(TAG, "ESP_GATT_PREP_WRITE_CANCEL");
        }
        if (prepare_write_env.prepare_buf)
        {
            free(prepare_write_env.prepare_buf);
            prepare_write_env.prepare_buf = NULL;
        }
        prepare_write_env.prepare_len = 0;
    }
    break;
    case ESP_GATTS_MTU_EVT: {
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
    }
    break;
    case ESP_GATTS_CONF_EVT: {
        ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
    }
    break;
    case ESP_GATTS_UNREG_EVT: {
    }
    break;
    case ESP_GATTS_CREATE_EVT: {
    }
    break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT: {
    }
    break;
    case ESP_GATTS_ADD_CHAR_EVT: {
    }
    break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
    }
    break;
    case ESP_GATTS_DELETE_EVT: {
    }
    break;
    case ESP_GATTS_START_EVT: {
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status,
                 param->start.service_handle);
    }
    break;
    case ESP_GATTS_STOP_EVT: {
    }
    break;
    case ESP_GATTS_CONNECT_EVT: {
        ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
        esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);
        esp_ble_conn_update_params_t conn_params{};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the iOS system, please refer to Apple official documents about the BLE connection parameters
         * restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
        // start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
    }
    break;
    case ESP_GATTS_DISCONNECT_EVT: {
        ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
        esp_ble_gap_start_advertising(&adv_params);
    }
    break;
    case ESP_GATTS_OPEN_EVT: {
    }
    break;
    case ESP_GATTS_CANCEL_OPEN_EVT: {
    }
    break;
    case ESP_GATTS_CLOSE_EVT: {
    }
    break;
    case ESP_GATTS_LISTEN_EVT: {
    }
    break;
    case ESP_GATTS_CONGEST_EVT: {
    }
    break;
    case ESP_GATTS_RESPONSE_EVT: {
    }
    break;
    case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
        if (param->add_attr_tab.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
        }
        else if (param->add_attr_tab.num_handle != HRS_IDX_NB)
        {
            ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)",
                     param->add_attr_tab.num_handle, HRS_IDX_NB);
        }
        else
        {
            ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d",
                     param->add_attr_tab.num_handle);
            memcpy(handle_table, param->add_attr_tab.handles, sizeof(handle_table));
            esp_ble_gatts_start_service(handle_table[IDX_SVC]);
        }
    }
    break;
    case ESP_GATTS_SET_ATTR_VAL_EVT: {
    }
    break;
    case ESP_GATTS_SEND_SERVICE_CHANGE_EVT: {
    }
    break;
        break;
    }
}

class z_ble_controller
{
  public:
    z_ble_controller()
    {
        profile_tab.gatts_if = ESP_GATT_IF_NONE;

        adv_config_done = 0;
        char_a_buf.fill(0);
        char_a_cfg_buf.fill(0);
    }

    void start()
    { // init
        ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        auto ret = esp_bt_controller_init(&bt_cfg);
        if (ret)
        {
            ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            return;
        }
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret)
        {
            ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
            return;
        }
        esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
        ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
        if (ret)
        {
            ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
            return;
        }
        ret = esp_bluedroid_enable();
        if (ret)
        {
            ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
            return;
        }
        ret = esp_ble_gatts_register_callback(gatts_event_handler);
        if (ret)
        {
            ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
            return;
        }
        ret = esp_ble_gap_register_callback(gap_event_handler);
        if (ret)
        {
            ESP_LOGE(TAG, "gap register error, error code = %x", ret);
            return;
        }
        ret = esp_ble_gatts_app_register(ESP_APP_ID);
        if (ret)
        {
            ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
            return;
        }
        esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
        if (local_mtu_ret)
        {
            ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
        }
    }

  private:
};

#endif