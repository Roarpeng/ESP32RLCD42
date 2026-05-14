# ESP32RLCD42 — 小智 AI 语音桌面助手

基于 **Waveshare ESP32-S3-RLCD-4.2** 开发板的智能语音桌面设备，搭载 400×300 反射式单色 RLCD 屏幕，集成小智 AI 语音交互、五桌面切换、电子相册、番茄钟、天气显示等功能。

## 硬件平台

| 参数 | 规格 |
|------|------|
| MCU | ESP32-S3-WROOM-1-N16R8（双核 Xtensa LX7 @ 240MHz） |
| Flash / PSRAM | 16MB / 8MB |
| 显示屏 | 400×300 RLCD，1-bit 黑白反射式液晶 |
| 音频输入 | ES7210（四通道 ADC） |
| 音频输出 | ES8311 + MAX98357A 功放 |
| 温湿度传感器 | SHTC3（I2C） |
| 实时时钟 | PCF85063（I2C） |
| SD 卡 | SDMMC，用于照片和白噪音文件 |
| 电池检测 | ADC GPIO4 |
| 按键 | BOOT（GPIO0）+ USER（GPIO18） |

## 功能概览

### 五桌面循环

USER 键单击循环切换，60 秒无操作自动回到时钟首页：

```
⏰ 时钟（默认首页）→ 🌤 天气 → 💬 格言 → 🖼 相册 → 🍅 番茄钟
```

| 桌面 | 内容 | USER 双击操作 |
|------|------|--------------|
| **时钟** | 翻页时钟 + 日期 + 温度 + 备忘/天气动态信息 | 刷新数据 |
| **天气** | 大字温度 + 天气描述 + AI 对话卡片 + 备忘录 | 刷新数据 |
| **格言** | 每日格言（AI 推送） + 状态栏 | 刷新数据 |
| **相册** | SD 卡 JPEG 照片轮播 / 无图时显示 Web 上传地址 | 下一张照片 |
| **番茄钟** | 倒计时 + 状态文字 + 操作提示 | 启动/暂停 |

### AI 语音交互

- **BOOT 键**单击：开始/结束 AI 对话
- 唤醒词："你好小智"
- 支持 WebSocket / MQTT 双工通信
- MCP 工具：天气写入、桌面切换、番茄钟控制、备忘录增删、相册管理

### 电子相册

- SD 卡 `/sdcard/photos/` 目录放入 JPEG 图片
- 通过 Web 浏览器上传（WiFi 连接后访问设备 IP）
- Floyd-Steinberg 抖动算法将照片转为 1-bit 黑白显示
- 支持自动轮播和手动翻页

### 番茄钟

- USER 双击启动（在番茄钟页面时）或语音说"开始番茄钟"
- 默认 25 分钟倒计时
- 支持白噪音（SD 卡 `/sdcard/white-noise/` 放入 MP3）
- 运行中免受自动回首页影响

## 按键交互

| 按键 | 操作 | 行为 |
|------|------|------|
| **BOOT** | 单击 | 开始/结束 AI 对话（启动时进入配网） |
| **USER** | 单击 | 切换下一个桌面 |
| **USER** | 双击 | 当前桌面的主操作（见上表） |
| **USER** | 长按 3 秒 | 启动时跳过配网 / 运行时显示系统信息 |

## 构建与烧录

### 环境要求

- **ESP-IDF v6.0.1**
- CMake ≥ 3.16.0
- Python 3.8+
- Xtensa ESP32-S3 交叉编译工具链

### 构建

```bash
# 设置 ESP-IDF 环境
export IDF_PATH=/path/to/esp-idf-v6.0.1
source $IDF_PATH/export.sh

# 编译
cd ESP32RLCD42
idf.py build
```

构建产物：`build/ESP32RLCD42.bin`（约 4.7 MB）

### 烧录

```bash
idf.py -p /dev/ttyUSB0 flash

# 或手动指定参数
python -m esptool --chip esp32s3 -p /dev/ttyUSB0 -b 460800 \
  --before default-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x20000 build/ESP32RLCD42.bin \
  0xa00000 build/generated_assets.bin
```

### 串口监视

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## Flash 分区

| 分区 | 类型 | 偏移 | 大小 | 用途 |
|------|------|------|------|------|
| nvs | data | 0x9000 | 20 KB | WiFi 凭据、备忘录、配置 |
| otadata | data | 0xE000 | 8 KB | OTA 数据 |
| phy_init | data | 0x10000 | 4 KB | PHY 校准 |
| factory | app | 0x20000 | 5.88 MB | 固件 |
| assets | data/spiffs | 0x600000 | 2 MB | 字体、表情素材 |

## 项目结构

```
ESP32RLCD42/
├── src/                          # 应用源码
│   ├── boards/
│   │   ├── waveshare-s3-rlcd-4.2/   # 目标板卡
│   │   │   ├── config.h              # 引脚定义
│   │   │   ├── waveshare-s3-rlcd-4.2.cc  # Board 入口、按键、MCP 工具
│   │   │   ├── custom_lcd_display.*  # 5 桌面 UI 核心
│   │   │   ├── rlcd_driver.*         # RLCD SPI 驱动
│   │   │   ├── data_update_task.cc   # 后台数据刷新
│   │   │   ├── weather_ui.cc         # 天气桌面
│   │   │   ├── pomodoro_ui.cc        # 番茄钟桌面
│   │   │   ├── music_ui.cc           # 音乐页（保留未启用）
│   │   │   ├── managers/             # 传感器/天气/番茄钟/SD卡管理
│   │   │   └── assets/fonts/         # 板卡专用字体
│   │   └── common/                   # 通用板卡代码
│   ├── audio/                    # 音频编解码、唤醒词、处理器
│   ├── display/                  # 显示抽象层
│   ├── photo/                    # 电子相册（JPEG 解码、HTTP 上传）
│   ├── protocols/                # WebSocket / MQTT 协议
│   ├── application.*             # 主事件循环
│   └── mcp_server.*              # MCP 工具注册
├── components/                   # 本地组件
├── managed_components/           # IDF 组件管理器自动下载（勿手动修改）
├── partitions/                   # Flash 分区表
├── scripts/                      # 构建辅助脚本
│   ├── convert_emoji_to_1bit.py  # emoji 黑白化（构建时自动运行）
│   └── build_default_assets.py   # 资产打包
├── sdkconfig                     # ESP-IDF 构建配置
├── CMakeLists.txt                # 项目根 CMake
└── spec.md                       # 详细产品规格说明
```

## MCP 工具列表

| 工具 | 功能 | 示例语音指令 |
|------|------|-------------|
| `self.system.info` | 查询系统信息 | "系统状态" |
| `self.weather.update` | 写入天气数据 | （AI 自动调用） |
| `self.disp.switch` | 切换桌面 | "打开时钟页"、"切到相册" |
| `self.disp.network` | 重新配网 | "重新配网" |
| `self.pomodoro.start` | 启动番茄钟 | "开始番茄钟"、"专注 25 分钟" |
| `self.pomodoro.stop` | 停止番茄钟 | "停止番茄钟" |
| `self.pomodoro.pause` | 暂停/恢复 | "暂停番茄钟"、"继续" |
| `self.pomodoro.status` | 查询状态 | "还剩多少时间" |
| `self.memo.add` | 添加备忘录 | "帮我记一下明天开会" |
| `self.memo.list` | 查看备忘录 | "我有什么备忘" |
| `self.memo.done` | 完成备忘 | "第一条备忘完成了" |
| `self.memo.clear` | 清空备忘 | "清空所有备忘" |
| `self.photo.switch` | 切到相册 | "打开相册" |
| `self.photo.next` | 下一张照片 | "下一张" |
| `self.photo.prev` | 上一张照片 | "上一张" |
| `self.photo.interval` | 设置轮播间隔 | "每 30 秒换一张" |

## 配网流程

1. 设备开机进入配网模式，屏幕显示 QR 码和热点名称
2. 手机连接设备热点，扫码或访问 `http://192.168.4.1`
3. 输入家庭 WiFi 的 SSID 和密码
4. 配网成功后自动同步时间、获取天气、进入桌面
5. 长按 USER 键 3 秒可跳过配网直接进入本地桌面

## 数据存储

- **NVS**：WiFi 凭据、备忘录 JSON、设备配置
- **SPIFFS Assets**：字体、emoji 素材（mmap 内存映射加载）
- **SD 卡**：`/sdcard/photos/`（相册照片）、`/sdcard/white-noise/`（白噪音 MP3）

## 技术优化

- **Emoji 黑白化**：42 个 emoji 从 RGB565A8 自动转为 1-bit I1 格式，节省约 300 KB Flash
- **省电模式**：5 分钟无活动降低刷新频率（1 秒 → 5 秒）
- **Floyd-Steinberg 抖动**：照片在单色屏上呈现最佳黑白效果
- **RLCD flush**：LVGL RGB565 → 1-bit 实时转换 + LUT 加速像素映射

## 致谢

- [小智 AI (Xiaozhi)](https://github.com/78/xiaozhi-esp32) — 核心语音交互框架
- [Waveshare ESP32-S3-RLCD-4.2](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2) — 硬件参考设计
- [LVGL](https://lvgl.io/) — UI 框架
- [ESP-IDF](https://github.com/espressif/esp-idf) — 开发框架

## 许可证

本项目基于小智 ESP32 开源项目开发。
