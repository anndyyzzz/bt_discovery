# Example นี้มีชื่อว่า bt_discovery
โดยการทำงานของ Example นี้คือการค้นหาอุปกรณ์ที่รองรับ Classic Bluetooth แล้วเก็บชื่อ Bluetooth และเก็บบริการต่างๆที่อุปกรณ์สามารถรองรับได้
#
ขั้นตอนแรกเลือก Example
# ![Screenshot 2024-11-10 004926](https://github.com/user-attachments/assets/89c66a99-a04d-41bf-9f35-7457195ec65c)

รันและbuildโปรแกรม
# ![Screenshot 2024-11-10 005743](https://github.com/user-attachments/assets/89b4170d-ec35-41ec-bb37-36d43dd4b6ed)

เปิด Bluetooth ในมือถือแล้วกด reset ที่บอร์ดเพื่อให้บอร์ดค้นหาอุปกรณ์ใหม่
# ![Screenshot 2024-11-10 005825](https://github.com/user-attachments/assets/03ed8e26-5c16-4244-b5ba-6bc7ed7f3b3c)

# แก้ไขเพิ่มเติม
จาก code จะสแกนหาได้เพียงมือถือและ Audio/Video
# static void update_device_info(esp_bt_gap_cb_param_t *param)
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
                
                // ตรวจสอบว่าเป็นประเภทมือถือหรือ Audio/Video
                if (esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PHONE ||
                    esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_AV) {
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

    // ถ้าเป็นอุปกรณ์ที่ตรงตามเงื่อนไขมือถือหรือ Audio/Video
    if (is_target_device && !m_dev_info.dev_found) {
        m_dev_info.dev_found = true;
        m_dev_info.cod = cod;
        m_dev_info.state = APP_GAP_STATE_DEVICE_DISCOVER_COMPLETE;
        ESP_LOGI(GAP_TAG, "การค้นหาอุปกรณ์เสร็จสิ้น: %s", m_dev_info.bdname);
        ESP_LOGI(GAP_TAG, "หยุดการค้นหาอุปกรณ์ ...");
        esp_bt_gap_cancel_discovery();
    }
}

# ![Screenshot 2024-11-10 011432](https://github.com/user-attachments/assets/1d6da489-498e-4f5e-9150-18a8f040c80f)
