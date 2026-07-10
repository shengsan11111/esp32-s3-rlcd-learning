# ESP32-S3-RLCD-Learning

> 板子：Waveshare ESP32-S3-RLCD-4.2
> 环境：ESP-IDF v5.5.0+
> 定位：从 I2C 物理层 → SHTC3 → RLCD 驱屏 → ... 逐步啃

## 📚 学习目录

| 序号 | 工程 | 说明 |
|------|------|------|
| 01 | [i2c_shtc3_simple](01_i2c_shtc3_simple/) | SHTC3 温湿度检测，新 I2C Master 驱动 |
| 02 | [rlcd_display](02_rlcd_display/) | RLCD 4.2" 驱屏（待添加） |
| 03 | xxx | （待添加） |

## 🛠️ 环境速记

- SHTC3：SCL@GPIO14, SDA@GPIO13, I2C_NUM_0, 400kHz
- RLCD：SPI 接口，具体引脚见各工程 README
- 其他 GPIO 分配见 Waveshare 官方原理图

## 📖 编译运行

每个工程独立编译：

```bash
cd 01_i2c_shtc3_simple
idf.py build flash monitor
```

## 📝 学习笔记

- [01 SHTC3温湿度检测](docs/01_SHTC3温湿度检测.md)
