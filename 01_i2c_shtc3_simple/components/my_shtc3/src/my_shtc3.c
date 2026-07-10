#include "my_shtc3.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char *TAG = "my_shtc3";

#define SHTC3_PETP_VOL 4.0f

static uint8_t shtc3_check_crc(uint8_t data[], uint8_t len, uint8_t crc)
{
    uint8_t bit;
    uint8_t calc_crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        calc_crc ^= data[i];
        for (bit = 8; bit > 0; bit--) {
            calc_crc = (calc_crc << 1) ^ ((calc_crc & 0x80) ? 0x131 : 0);
        }
    }
    return (calc_crc == crc) ? 0 : 1;
}

static float shtc3_calc_temp(uint16_t raw)
{
    return 175.0f * raw / 65536.0f - 45.0f - SHTC3_PETP_VOL;
}

static float shtc3_calc_humi(uint16_t raw)
{
    return 100.0f * raw / 65536.0f;
}

static esp_err_t shtc3_write_cmd(shtc3_handle_t *handle, uint16_t cmd)
{
    uint8_t buf[2] = {cmd >> 8, cmd & 0xFF};
    return i2c_master_transmit(handle->dev_handle, buf, 2, 1000);
}

static esp_err_t shtc3_read(shtc3_handle_t *handle, uint8_t *buf, size_t len)
{
    return i2c_master_receive(handle->dev_handle, buf, len, 1000);
}

esp_err_t shtc3_init(shtc3_handle_t *handle, i2c_master_bus_handle_t bus_handle)
{
    esp_err_t ret;
    
    if (!handle || !bus_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handle->bus_handle = bus_handle;
    
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_ADDR,
        .scl_speed_hz = 400000,
    };
    
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &handle->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SHTC3 device");
        return ret;
    }
    
    ret = shtc3_write_cmd(handle, SHTC3_WAKEUP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "First wakeup may have failed");
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ret = shtc3_write_cmd(handle, SHTC3_SOFT_RESET);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soft reset failed");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = shtc3_write_cmd(handle, SHTC3_WAKEUP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wakeup after reset failed");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    uint16_t id;
    ret = shtc3_get_id(handle, &id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device ID");
        return ret;
    }
    
    handle->device_id = id;
    handle->initialized = 1;
    
    ESP_LOGI(TAG, "SHTC3 initialized successfully, ID: 0x%04X", id);
    return ESP_OK;
}

esp_err_t shtc3_deinit(shtc3_handle_t *handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->dev_handle) {
        i2c_master_bus_rm_device(handle->dev_handle);
        handle->dev_handle = NULL;
    }
    
    handle->initialized = 0;
    ESP_LOGI(TAG, "SHTC3 deinitialized");
    return ESP_OK;
}

esp_err_t shtc3_get_id(shtc3_handle_t *handle, uint16_t *id)
{
    esp_err_t ret;
    uint8_t buf[3] = {0};
    
    if (!handle || !id || !handle->dev_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ret = shtc3_write_cmd(handle, SHTC3_READ_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send READ_ID command");
        return ret;
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
    ret = shtc3_read(handle, buf, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ID data");
        return ret;
    }
    
    if (shtc3_check_crc(buf, 2, buf[2])) {
        ESP_LOGE(TAG, "ID CRC error");
        return ESP_ERR_INVALID_CRC;
    }
    
    *id = (buf[0] << 8) | buf[1];
    return ESP_OK;
}

esp_err_t shtc3_read_temp_humi(shtc3_handle_t *handle, shtc3_data_t *data)
{
    esp_err_t ret;
    uint8_t buf[6] = {0};
    uint16_t raw_temp, raw_humi;
    
    if (!handle || !data || !handle->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ret = shtc3_write_cmd(handle, SHTC3_WAKEUP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wakeup failed");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ret = shtc3_write_cmd(handle, SHTC3_MEAS_CLOCKSTR_RH_T);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Measure command failed");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ret = shtc3_read(handle, buf, 6);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed");
        return ret;
    }
    
    if (shtc3_check_crc(buf, 2, buf[2]) || shtc3_check_crc(&buf[3], 2, buf[5])) {
        ESP_LOGE(TAG, "Data CRC error");
        return ESP_ERR_INVALID_CRC;
    }
    
    raw_humi = (buf[0] << 8) | buf[1];
    raw_temp = (buf[3] << 8) | buf[4];
    
    data->temperature = shtc3_calc_temp(raw_temp);
    data->humidity = shtc3_calc_humi(raw_humi);
    
    ret = shtc3_write_cmd(handle, SHTC3_SLEEP);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Sleep failed");
    }
    
    return ESP_OK;
}

esp_err_t shtc3_sleep(shtc3_handle_t *handle)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return shtc3_write_cmd(handle, SHTC3_SLEEP);
}

esp_err_t shtc3_wakeup(shtc3_handle_t *handle)
{
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return shtc3_write_cmd(handle, SHTC3_WAKEUP);
}