# ESP32RLCD42 项目规格说明

## 1. 项目概述

本项目是基于 Waveshare ESP32-S3-RLCD-4.2 开发板的智能语音桌面设备。设备使用 400x300 反射式单色 RLCD 屏幕，集成小智 AI 语音交互、扫码配网、天气/时间/温湿度显示、番茄钟、备忘录、电子相册和系统信息查看等能力。

当前固件重点面向低功耗黑白桌面体验：UI 使用 LVGL 绘制，风格参考电子墨水屏，主交互通过 BOOT 键、USER 键、扫码配网和语音 MCP 工具完成。

目标启动体验：

- 开机默认进入配网引导。
- 长按 USER 键 3 秒可以跳过本次配网，直接进入本地桌面。
- 配网成功后自动同步时间、天气和每日格言。
- 翻到电子相册桌面时，如果设备内没有照片，则显示相册 Web 上传地址，并提示用户通过 Web 上传照片。

## 2. 硬件平台

目标板卡：`waveshare-s3-rlcd-4.2`

核心硬件：

- MCU：ESP32-S3-WROOM-1-N16R8
- Flash：16MB
- PSRAM：8MB
- 显示屏：400x300 RLCD，1-bit 黑白反射式液晶
- 音频输入：ES7210
- 音频输出：ES8311 + MAX98357A
- 温湿度：SHTC3，I2C
- RTC：PCF85063，I2C
- SD 卡：SDMMC，用于白噪音和照片文件
- 电池检测：ADC GPIO4
- BOOT 键：GPIO0
- USER 键：GPIO18

关键引脚定义位于：

- `src/boards/waveshare-s3-rlcd-4.2/config.h`

## 3. 软件架构

项目使用 ESP-IDF + LVGL。主要模块如下：

- 板级入口：`src/boards/waveshare-s3-rlcd-4.2/waveshare-s3-rlcd-4.2.cc`
- 显示类：`src/boards/waveshare-s3-rlcd-4.2/custom_lcd_display.h/.cc`
- RLCD 驱动：`src/boards/waveshare-s3-rlcd-4.2/rlcd_driver.h/.cc`
- 数据刷新任务：`src/boards/waveshare-s3-rlcd-4.2/data_update_task.cc`
- 天气桌面：`src/boards/waveshare-s3-rlcd-4.2/weather_ui.cc`
- 专注桌面：`src/boards/waveshare-s3-rlcd-4.2/pomodoro_ui.cc`
- 音乐页旧实现：`src/boards/waveshare-s3-rlcd-4.2/music_ui.cc`
- 番茄钟管理：`src/boards/waveshare-s3-rlcd-4.2/managers/pomodoro_manager.*`
- 传感器管理：`src/boards/waveshare-s3-rlcd-4.2/managers/sensor_manager.*`
- 天气缓存管理：`src/boards/waveshare-s3-rlcd-4.2/managers/weather_manager.*`
- 照片管理：`src/photo/*`

LVGL flush 回调将 RGB565 渲染结果转换为 RLCD 的黑白像素，并通过 `RlcdDriver` 刷新到屏幕。

## 4. 当前 5 个桌面

USER 键单击循环切换 5 个桌面：

1. 格言桌面
2. 电子相册桌面
3. 天气桌面
4. 专注/番茄钟桌面
5. 翻页时钟桌面

切换顺序：

```text
格言 -> 相册 -> 天气 -> 专注 -> 时钟 -> 格言
```

### 4.1 格言桌面

显示内容：

- 顶部状态栏：WiFi、电量、温度、湿度
- 每日格言文本
- `NEW QUOTE` 按钮视觉元素
- 盆栽和爪印装饰图形

数据行为：

- 配网成功后自动获取每日格言。
- 格言桌面应显示最新每日格言；若获取失败，显示本地默认格言。

实现位置：

- `CustomLcdDisplay::SetupQuoteUI()`

### 4.2 电子相册桌面

显示内容：

- 标题：`* 电子相册 *`
- 中央黑白插画式照片区域
- 页码：`1 / 28`
- 左右翻页箭头

当前实现为 LVGL 基础图形绘制的相册桌面。项目中仍保留 `src/photo/*` 的照片管理、HTTP 上传和轮播能力，后续可以继续把真实 SD 卡照片接回此桌面。

目标行为：

- 翻页到电子相册桌面时检查相册是否有图片。
- 如果有图片，显示当前照片、页码和左右翻页控件。
- 如果没有图片，显示相册 Web 地址，并提示：`请通过 Web 上传照片`。
- Web 地址应来自设备实际 HTTP 服务地址，优先显示局域网 IP；未联网时显示热点或本机可访问地址。

实现位置：

- `CustomLcdDisplay::SetupPhotoDesktopUI()`
- `src/photo/http_server.cc`
- `src/photo/photo_manager.*`

### 4.3 天气桌面

显示内容：

- 顶部状态栏：WiFi、电量、温度、湿度
- 大号温度：如 `26°C`
- 天气描述：如 `Cloudy`
- 位置：`Shenzhen, Nanshan`
- 右侧指标：体感温度、湿度、空气质量
- 底部小时刻度

动态数据来源：

- 温湿度来自 SHTC3
- 天气文本和温度来自 `WeatherManager` 的缓存，推荐由 AI/MCP 调用 `self.weather.update` 写入
- 目标行为：配网成功后自动获取天气信息并刷新该桌面

实现位置：

- `weather_ui.cc`
- `data_update_task.cc`

### 4.4 专注/番茄钟桌面

显示内容：

- 顶部状态栏
- 标语：`FOCUS ON THE NOW, MEET A BETTER SELF`
- 中央 `25:00` 倒计时
- `Start Focus` 按钮视觉元素
- 底部四个功能区：Tasks、Noise、Forest、Stats

番茄钟状态由 `PomodoroManager` 管理，倒计时运行时每秒刷新 `pomo_countdown_label_`。

实现位置：

- `pomodoro_ui.cc`
- `managers/pomodoro_manager.*`
- `data_update_task.cc`

### 4.5 翻页时钟桌面

显示内容：

- 顶部状态栏
- 左侧翻页时钟：小时、分钟、AM/PM、秒
- 右侧日期：如 `06/12 Wed`
- 右侧温度：如 `26.5°C`
- 爱因斯坦格言

动态数据来源：

- 时间来自 NTP/RTC
- 温度来自 SHTC3

实现位置：

- `CustomLcdDisplay::SetupClockUI()`
- `data_update_task.cc`

## 5. 按键交互

BOOT 键：

- 启动阶段单击：进入 WiFi 配网
- 正常运行单击：切换 AI 对话状态，开始或停止语音交互

USER 键：

- 单击：循环切换 5 个桌面
- 双击：刷新数据，包括时间、传感器和天气缓存相关逻辑
- 开机配网阶段长按 3 秒：跳过本次配网，进入本地桌面
- 正常运行阶段长按：显示系统信息

实现位置：

- `CustomBoard::InitializeButtons()`

## 6. WiFi 配网

目标配网方式为 ESP32 小程序扫码配网。

开机行为：

- 设备开机后默认进入配网引导。
- 如果用户长按 USER 键 3 秒，则跳过本次配网。
- 跳过配网只影响本次启动，不应清除已保存的 WiFi 凭据。

扫码配网流程：

1. 设备启动配网热点。
2. 电子屏幕显示二维码、热点名和配网提示。
3. 手机扫码连接设备。
4. 手机进入 ESP32 小程序或配网页。
5. 用户输入目标 WiFi 的 SSID 和密码。
6. 设备保存 WiFi 凭据并连接网络。
7. 配网成功后自动隐藏二维码并进入桌面。

二维码内容：

- 目标应指向 ESP32 小程序/配网页入口，而不是只编码 WiFi 热点。
- 如果使用 Web 配网页，二维码内容应为设备配网页 URL，例如 `http://192.168.4.1`。
- 如果使用小程序 scheme 或二维码参数，应包含设备热点 SSID 或设备标识，方便小程序连接设备并下发 WiFi 凭据。

配网成功后的自动动作：

- 同步 NTP 时间。
- 获取天气信息。
- 获取每日格言。
- 刷新 5 个桌面的状态栏和对应内容。

相关实现：

- `src/boards/common/wifi_board.cc`
- `Display::ShowWifiProvisioningQr()`
- `CustomLcdDisplay::ShowWifiProvisioningQr()`
- `CustomLcdDisplay::HideWifiProvisioningQr()`

当前代码状态：

- 已有扫码配网页面和二维码显示能力。
- 当前二维码实现偏向设备热点连接；需要按本规格调整为 ESP32 小程序/配网页入口二维码。
- 当前开机逻辑仍会优先尝试已有 WiFi；需要按本规格调整为开机默认配网，并支持 USER 长按 3 秒跳过。

## 7. AI / MCP 工具

板级 MCP 工具注册在：

- `src/boards/waveshare-s3-rlcd-4.2/waveshare-s3-rlcd-4.2.cc`

主要工具：

- `self.system.info`：查询 CPU、内存、电池、WiFi 状态等系统信息
- `self.weather.update`：写入天气数据到设备缓存
- `self.disp.network`：重新进入配网模式
- `self.disp.switch`：切换显示页面
- `self.pomodoro.start`：启动番茄钟
- `self.pomodoro.stop`：停止番茄钟
- `self.pomodoro.status`：查询番茄钟状态
- `self.pomodoro.pause`：暂停或恢复番茄钟
- `self.memo.add`：添加备忘录
- `self.memo.list`：列出备忘录
- `self.memo.done`：完成并删除备忘录
- `self.memo.clear`：清空备忘录
- `self.photo.switch`：切换到相册模式
- `self.photo.next`：下一张照片
- `self.photo.prev`：上一张照片
- `self.photo.interval`：设置照片轮播间隔

注意：`self.disp.switch` 的描述中仍保留旧的 music/weather/pomodoro 文案，后续应更新为 5 桌面模式。

## 8. 数据存储

NVS：

- `wifi`：保存 WiFi 凭据
- `memo.items`：保存备忘录 JSON 数组
- 天气数据由 `WeatherManager` 缓存在运行态
- 每日格言：目标为配网成功后获取并缓存；缓存位置待定，可使用 NVS 或运行态缓存

备忘录格式：

```json
[
  {"t": "15:00", "c": "开会"},
  {"t": "", "c": "买牛奶"}
]
```

SD 卡：

- `/sdcard/white-noise/`：白噪音 MP3
- `/sdcard/photos/`：相册照片

## 9. 构建与烧录

目标：

```text
ESP32-S3
BOARD_TYPE = waveshare-s3-rlcd-4.2
```

已验证构建输出：

```text
build/ESP32RLCD42.bin
```

最近一次增量编译通过，固件大小：

```text
0x45cae0
```

最小 app 分区：

```text
0x4f0000
```

剩余空间约：

```text
12%
```

烧录串口：

```text
COM3
```

芯片信息：

```text
ESP32-S3
MAC: 44:1b:f6:8f:08:5c
```

烧录命令示例：

```powershell
cd build
C:\Espressif\tools\python\v6.0.1\venv\Scripts\python.exe -m esptool --chip esp32s3 -p COM3 -b 460800 --before default-reset --after hard-reset write-flash "@flash_args"
```

## 10. 已知注意事项

- 工程目录包含大量备份目录和 managed components，完整 CMake/graphify 扫描耗时很长。
- 新增源文件可能触发 CMake 重新生成，当前 5 桌面的新增实现被放在已有构建图中的 `custom_lcd_display.cc`，以便稳定增量构建。
- LVGL 字体字符集有限，UI 中应尽量避免依赖不确定的特殊符号；复杂图标优先用 LVGL 基础图形绘制。
- RLCD 为 1-bit 黑白屏，设计应使用高对比、大字号、少灰阶、清晰边框。
- 天气自动拉取目前不是主路径，建议由 AI 调用外部天气源后通过 `self.weather.update` 写入设备。
- 原音乐页代码仍保留，但当前 5 桌面循环中不再展示音乐页。
- 开机默认配网、USER 长按 3 秒跳过、配网成功后自动获取天气/格言、相册空状态 Web 上传提示，是新的目标行为，需要在代码中补齐或核对实现。

## 11. 后续建议

- 更新 `self.disp.switch` 工具，使其支持 `quote/photo/weather/pomodoro/clock/toggle`。
- 将相册桌面接回真实 `PhotoManager` 图片渲染，而不是当前静态 LVGL 图形。
- 相册无图时显示 Web 上传地址和上传提示。
- 实现开机默认配网，以及 USER 长按 3 秒跳过配网。
- 将二维码内容改为 ESP32 小程序/配网页入口。
- 配网成功后自动同步 NTP、天气和每日格言。
- 为 5 个桌面抽出公共状态栏绘制函数，减少 `custom_lcd_display.cc` 体积。
- 为格言桌面增加格言列表和随机/下一条逻辑。
- 为专注桌面底部 Tasks/Noise/Forest/Stats 增加实际状态联动。
