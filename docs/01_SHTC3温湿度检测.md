# 微雪 ESP32-S3-RLCD-4.2 板载 SHTC3 驱动

> 开发板：微雪 ESP32-S3-RLCD-4.2，SHTC3 板载焊死
> 环境：ESP-IDF v5.5.0+，使用 IDF v5 新 I2C Master 驱动（`i2c_new_master_bus` / `i2c_master_bus_add_device`）

## 第一节：前言
### 芯片介绍
- SHTC3是一款低功耗温湿度传感器，温度的采集范围为-40℃~125℃（精度 ±0.2°C），湿度的采集范围为 0%~100%（精度 ±2%RH）。
- I2C 接口，支持 400kHz 时钟频率。
- 地址：`0x70`（7 位从机地址），没有A0/A1引脚，固定地址。
- 只有一个十六位寄存器，无寄存器地址概念，属于"命令式"器件。

### 板载 SHTC3 驱动连接
- SHTC3 焊死在板子内部，不需飞线
- SCL → GPIO14，SDA → GPIO13 , VCC 和 GND 连接板载电源

## 第二节：I2C基础

- SHTC3 是 I2C slave-only，ESP32-S3 作 master，单主模型——总线上只有 ESP32-S3 会主动发 START，SHTC3 被动响应。
- 开漏 + 上拉 + 线与
  - SCL/SDA 均为开漏(OD) + 外部上拉，不是推挽： 
    - 任意设备拉低 → 总线低；全设备释放 → 上拉拉高（线与逻辑）
    - SDA 主从都能控（ACK 位就是从机拉低），所以必须 OD
    - SCL 虽由主机产，但从机可做 clock stretching（测量期间拉 SCL 低等），所以 SCL 也得 OD——单主模型下这条不是为主机服务，是给从机留"按暂停"的权力
- 400kHz 指 SCL 频率  
  - "Fast mode 400kHz"指的是 SCL 引脚上的时钟频率
  - i2c_master_bus_config_t.clk_source选 I2C_CLK_SRC_DEFAULT即走 APB，分频由 dev_cfg.scl_speed_hz = 400000 实现
- Polling vs Clock Stretching（跟 SHTC3 相关的选择）
  - SHTC3 测量期间对总线SCL的处理，有两种选择：

| 模式 | 测量期间 |
|------|----------|
| Polling | 从机对所有 I2C 寻址回 NACK，不会拉低 SCL|
| Clock Stretching | 从机 ACK 后拉 SCL 低 ~ 10ms，测完才释放 |

- 当前方案选择：Polling模式，主机等待50ms后读取结果

## 第三节：SHTC3 指令集
- SHTC3 没有寄存器地址概念，整颗芯片只有一个 16 位"命令寄存器"，所有操作都是主机发 16bit 命令 → 从机执行。这跟 EEPROM、OLED、BME280 那种"先写寄存器地址、再写/读数据"的寄存器式模型不一样。
### 3.1 基础指令
|指令|编码|作用|
|------|------|------|
|软复位|0x805D	|复位内部状态机，上电后可发一次|
|唤醒|0x3517	|睡眠态 → 工作态，必须 wake 才能发测量|
|睡眠|0xB098	|工作态 → 睡眠，电流 < 1μA|
|读 ID|0xEFC8	|回 2 字节 ID + 1 字节 CRC，可验通信|
|测量|见下表	|温湿度一起出，8 选 1|   
- 指令全 16bit，高字节先发，代码里 cmd >> 8和 cmd & 0xFF拆开发

### 3.2 测量指令
- T-first / RH-first：温度/湿度先出（当前方案选择：T-first）
- Normal / LowPower：正常模式/低功耗模式
- Polling / Clock Stretching：轮询模式/时钟拉低模式

|T-first, Normal|RH-first, Normal|T-first, LowPower|RH-first, LowPower|
|------|------|------|------|
|Polling|0x7866	|0x58E0	|0x609C	|0x401A|
|Clock Stretching|0x7CA2	|0x5C24	|0x6458	|0x44DE|

### 3.3 ESP32 侧 write_cmd封装
- 封装 i2c_master_transmit，返回 ESP_OK 成功
  - 发送 2 字节命令(high byte先发)
- 失败返回错误码
```c
static esp_err_t shtc3_write_cmd(shtc3_handle_t *handle, uint16_t cmd)
{
    uint8_t buf[2] = {cmd >> 8, cmd & 0xFF};
    return i2c_master_transmit(handle->dev_handle, buf, 2, pdMS_TO_TICKS(1000));
}
```

## 第四节：采集流程
- SHTC3 标准测量周期 datasheet 给的是四步：唤醒 → 测量 → 读数据 → 睡眠。
  - 每一步都是独立的 I2C 事务（各自 START → STOP）
### 4.1 唤醒
- 发送唤醒指令 `0x3517`
- 等待 50ms，确保从机进入工作态
```c
ret = shtc3_write_cmd(handle, SHTC3_WAKEUP);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Wakeup failed");
    return ret;
}
vTaskDelay(pdMS_TO_TICKS(50));
```
### 4.2 测量
- 发送测量指令 `0x7866`（Polling模式） ---  也可以选用其他模式
  - Polling 模式下从机不拉 SCL，对所有 I2C 寻址回 NACK，直到测完数据
- 等待 50ms，确保从机完成测量
```c
ret = shtc3_write_cmd(handle, SHTC3_MEASURE);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Measure command failed");
    return ret;
}
vTaskDelay(pdMS_TO_TICKS(50));
```

### 4.3 读数据并校验 CRC
- 测量指令为 `0x7866`（Polling模式）
  - 读取 6 字节数据格式：[T_H][T_L][T_CRC][RH_H][RH_L][RH_CRC]
```c
ret = shtc3_read(handle, buf, 6);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Read failed");
    return ret;
}
// 校验数据 CRC
if (shtc3_check_crc(buf, 2, buf[2]) || shtc3_check_crc(&buf[3], 2, buf[5])) {
    ESP_LOGE(TAG, "Data CRC error");
    return ESP_ERR_INVALID_CRC;
}

raw_temp = (buf[0] << 8) | buf[1];
raw_humi = (buf[3] << 8) | buf[4];
// 计算温度和湿度值
// 温度：175.0f * raw / 65536.0f - 45.0f
// 湿度：100.0f * raw / 65536.0f
data->temperature = shtc3_calc_temp(raw_temp);
data->humidity = shtc3_calc_humi(raw_humi);
```

### 4.4 睡眠(退出工作态)
- 发送睡眠指令 `0xB098`
- 等待 50ms，确保从机进入睡眠态
```c
ret = shtc3_write_cmd(handle, SHTC3_SLEEP);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Sleep failed");
    return ret;
}
```

## 源码仓库 
- Gitee:[SHTC3 温湿度检测](https://gitee.com/lkj_2279840148/esp32-s3-rlcd-4.2-learning/tree/master/01_i2c_shtc3_simple)
- GitHub:[SHTC3 温湿度检测](https://github.com/shengsan11111/esp32-s3-rlcd-learning/tree/master/01_i2c_shtc3_simple)