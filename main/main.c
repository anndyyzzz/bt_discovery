#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"

#define GAP_TAG          "GAP"

// นิยามสถานะของ Bluetooth application
typedef enum {
    APP_GAP_STATE_IDLE = 0,
    APP_GAP_STATE_DEVICE_DISCOVERING,
    APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE,
    APP_GAP_STATE_SERVICE_DISCOVERING,
    APP_GAP_STATE_SERVICE_DISCOVER_COMPLETE,
} app_gap_state_t;

// นิยามโครงสร้างเพื่อจัดเก็บข้อมูลอุปกรณ์
typedef struct {
    bool dev_found;
    uint8_t bdname_len;
    uint8_t eir_len;
    uint8_t rssi;
    uint32_t cod;
    uint8_t eir[ESP_BT_GAP_EIR_DATA_LEN];
    uint8_t bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    esp_bd_addr_t bda;
    app_gap_state_t state;
} app_gap_cb_t;

static app_gap_cb_t m_dev_info;

// ฟังก์ชันแปลงที่อยู่ Bluetooth เป็นข้อความ
static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

// ฟังก์ชันจัดการผลลัพธ์การค้นหาอุปกรณ์
static void update_device_info(esp_bt_gap_cb_param_t *param)
{
    char bda_str[18];
    ESP_LOGI(GAP_TAG, "พบอุปกรณ์: %s", bda2str(param->disc_res.bda, bda_str, sizeof(bda_str)));

    uint32_t cod = 0;
    bool is_target_device = false;

    // ดึงข้อมูลอุปกรณ์
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *prop = param->disc_res.prop + i;
        switch (prop->type) {
            case ESP_BT_GAP_DEV_PROP_COD:
                cod = *(uint32_t *)(prop->val);
                ESP_LOGI(GAP_TAG, "--Class of Device: 0x%"PRIx32, cod);
                
                // ตรวจสอบว่าเป็นประเภทมือถือ, Audio/Video หรือ คอมพิวเตอร์
                if (esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PHONE ||
                    esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_AV ||
                    esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_COMPUTER) {
                    is_target_device = true;
                }
                break;
            case ESP_BT_GAP_DEV_PROP_RSSI:
                m_dev_info.rssi = *(int8_t *)(prop->val);
                break;
            case ESP_BT_GAP_DEV_PROP_BDNAME:
                m_dev_info.bdname_len = prop->len;
                memcpy(m_dev_info.bdname, prop->val, m_dev_info.bdname_len);
                m_dev_info.bdname[m_dev_info.bdname_len] = '\0';
                break;
            case ESP_BT_GAP_DEV_PROP_EIR:
                m_dev_info.eir_len = prop->len;
                memcpy(m_dev_info.eir, prop->val, m_dev_info.eir_len);
                break;
            default:
                break;
        }
    }

    // ถ้าเป็นอุปกรณ์ที่ตรงตามเงื่อนไขมือถือ, Audio/Video หรือ คอมพิวเตอร์
    if (is_target_device && !m_dev_info.dev_found) {
        m_dev_info.dev_found = true;
        m_dev_info.cod = cod;
        m_dev_info.state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
        ESP_LOGI(GAP_TAG, "การค้นหาอุปกรณ์เสร็จสิ้น: %s", m_dev_info.bdname);
        ESP_LOGI(GAP_TAG, "หยุดการค้นหาอุปกรณ์ ...");
        esp_bt_gap_cancel_discovery();
    }
}

// ฟังก์ชัน callback ของ GAP
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            update_device_info(param); // เรียกใช้ update_device_info เมื่อพบอุปกรณ์ใหม่
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                ESP_LOGI(GAP_TAG, "หยุดการค้นหาอุปกรณ์แล้ว");
                if (m_dev_info.dev_found) {
                    m_dev_info.state = APP_GAP_STATE_SERVICE_DISCOVERING;
                    ESP_LOGI(GAP_TAG, "ค้นหาบริการ...");
                    esp_bt_gap_get_remote_services(m_dev_info.bda);
                }
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                ESP_LOGI(GAP_TAG, "เริ่มการค้นหาอุปกรณ์...");
            }
            break;
        default:
            ESP_LOGI(GAP_TAG, "เหตุการณ์ที่เกิดขึ้น: %d", event);
            break;
    }
}

// ฟังก์ชันเริ่มต้นการใช้งาน Bluetooth
static void bt_app_gap_start_up(void)
{
    /* Register GAP callback function */
    esp_bt_gap_register_callback(bt_app_gap_cb);

    char *dev_name = "ESP_GAP_INQUIRY";
    esp_bt_gap_set_device_name(dev_name);

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    m_dev_info.state = APP_GAP_STATE_DEVICE_DISCOVERING;
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}

// ฟังก์ชันเริ่มต้นหลักของแอปพลิเคชัน
void app_main(void)
{
    char bda_str[18] = {0};
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_LOGI(GAP_TAG, "ที่อยู่ของอุปกรณ์: %s", bda2str(esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));

    bt_app_gap_start_up();
}
