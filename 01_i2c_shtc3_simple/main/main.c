#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "my_shtc3.h"

#define SHTC3_SCL_PIN 14
#define SHTC3_SDA_PIN 13
#define I2C_PORT_NUM 0

static const char *TAG = "main";
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static shtc3_handle_t shtc3_handle = {0};

static void shtc3_loop_task(void *arg)
{
    shtc3_data_t data;
    
    for (;;) {
        if (shtc3_read_temp_humi(&shtc3_handle, &data) == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %.2f°C, Humi: %.2f%%", data.temperature, data.humidity);
        } else {
            ESP_LOGE(TAG, "Failed to read temp/humi");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing I2C bus...");
    
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT_NUM,
        .scl_io_num = SHTC3_SCL_PIN,
        .sda_io_num = SHTC3_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ret = i2c_new_master_bus(&bus_cfg, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus");
        return;
    }
    
    ESP_LOGI(TAG, "Initializing SHTC3...");
    
    ret = shtc3_init(&shtc3_handle, i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SHTC3");
        return;
    }
    
    ESP_LOGI(TAG, "SHTC3 initialized successfully, starting loop task...");
    
    xTaskCreate(shtc3_loop_task, "shtc3_loop", 4096, NULL, 2, NULL);
}