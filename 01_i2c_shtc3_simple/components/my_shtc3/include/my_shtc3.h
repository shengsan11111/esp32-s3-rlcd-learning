#ifndef MY_SHTC3_H
#define MY_SHTC3_H

#include <stdint.h>
#include <driver/i2c_master.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHTC3_ADDR          0x70
#define SHTC3_READ_ID       0xEFC8
#define SHTC3_SOFT_RESET    0x805D
#define SHTC3_WAKEUP        0x3517
#define SHTC3_SLEEP         0xB098
#define SHTC3_MEAS_POLLING  0x7866
#define SHTC3_MEAS_CLOCKSTR_RH_T 0x5C24

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    uint16_t device_id;
    uint8_t initialized;
} shtc3_handle_t;

typedef struct {
    float temperature;
    float humidity;
} shtc3_data_t;

esp_err_t shtc3_init(shtc3_handle_t *handle, i2c_master_bus_handle_t bus_handle);
esp_err_t shtc3_deinit(shtc3_handle_t *handle);
esp_err_t shtc3_get_id(shtc3_handle_t *handle, uint16_t *id);
esp_err_t shtc3_read_temp_humi(shtc3_handle_t *handle, shtc3_data_t *data);
esp_err_t shtc3_sleep(shtc3_handle_t *handle);
esp_err_t shtc3_wakeup(shtc3_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif