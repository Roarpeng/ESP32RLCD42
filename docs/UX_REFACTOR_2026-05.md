# UI / UX 重构总结 (2026-05)

> 本文档归纳本次针对 **Waveshare ESP32-S3-RLCD-4.2 (Xiaozhi AI 语音助手)** 的两阶段 UI/UX 重构，便于后续维护者在不重新阅读全部 PR diff 的前提下快速建立心智模型。

## 1. 背景

| 维度 | 现状 |
|---|---|
| 硬件 | Waveshare ESP32-S3-RLCD-4.2，400×300 1-bit 单色反射式 LCD |
| 框架 | ESP-IDF v6.0.1 + LVGL 9 + 小智 (Xiaozhi) AI 协议栈 |
| 桌面 | 6 桌面轮播 (时钟 / 天气 / 格言 / 相册 / 番茄钟 / 音乐) + WiFi QR 配网页 + 设置/关于 modal |
| 后端 | 默认 `https://api.tenclass.net/xiaozhi/ota/`（不在本次重构范围内） |

## 2. 两阶段 PR

| 阶段 | 分支 | PR | 关注点 |
|---|---|---|---|
| **A. Pencil 对齐** | `cursor/align-ui-with-pencil-design-af12` | #2 | 把 7 个页面的真实渲染对齐到 `pencil-new.pen` 设计稿 |
| **B. UX P0–P3** | `cursor/ux-improvements-p0p3-af12` | #3 | 工业交互设计师视角的 13 项 UX 改进 |

## 3. 阶段 A：Pencil 对齐 (PR #2)

修复了 12 处「设计稿 vs 代码」偏差。摘要：

| # | 页面 | 修复 |
|---|---|---|
| 1 | Clock | 顶部分隔线由两条改为一条 |
| 2 | Weather | 顶部分隔线由两条改为一条 |
| 3 | Weather | 实心黑方块 → 用基本图元拼出云 + 太阳剪影 |
| 4 | Weather | 实心黑圆点 → map-pin 水滴形 |
| 5 | Weather | 右侧指标数值 24 px → 16 px (Pencil 标 20 px 等效) |
| 6 | Pomodoro | 手绘虚线段 → 实线圆角矩形 (radius 16, border 2 px) |
| 7 | Quote | NEW QUOTE 按钮加 refresh-cw 旋转刷新图标 |
| 8 | Photo | 「相册为空」应用 opacity 0.65 |
| 9 | Photo | `<` `>` 箭头从 24 px 收紧到 16 px |
| 10 | Photo | 「1 / 1」状态字 16 px → 14 px |
| 11 | Music | 唱片卡片 / 唱片盘加 2 px 白色边框 |
| 12 | Music | 唱片同心圆从 4 圈 → 3 圈 + 40 px 白心 + 10 px 黑孔 |

### 1-bit 单色屏关键约定

`Lvgl_flush_cb` 把 RGB565 阈值化时 `< 0x7FFF` → 黑、`>= 0x7FFF` → 白；opacity ≤ 0.5 的灰度像素都会被判为白色（不可见）。所以 Pencil 中很多 opacity 0.15 的灰线在 1-bit 屏上根本不会显示。统一用 1 px 实心黑线表达「单条分隔」，2 条 1 px 实心黑线表达「双条分隔」。这一约定写入 `DesktopHeaderSepSingle` / `DesktopHeaderSepDouble` 的注释。

## 4. 阶段 B：UX P0–P3 (PR #3)

按设计师严重程度排序的 13 项改进。

### 4.1 P0 致命体验

#### P0-1 AI 状态卡 8 态可视化

新增 `enum class AiBarStatus`：

```
OFFLINE / PROVISIONING / CONNECTING / ONLINE_IDLE
LISTENING / SPEAKING / UPGRADING / ERROR
```

以前 4 个文件里散落的 4 套 AI 卡构造代码统一成 `CustomLcdDisplay::BuildAiBar()`，handle 收纳到 `AiBarHandles ai_bars_[7]`（6 桌面 + WiFi QR）。`DataUpdateTask` 每次状态变化把 `DeviceState` 翻译成 `AiBarStatus` 调 `SetAiBarStatusAll`。每态对应不同 icon 形态：

| 状态 | icon | 角标 | 默认文案 |
|---|---|---|---|
| OFFLINE | 空心圆 + 斜杠 | — | 未联网 · 长按 USER 看详情 |
| PROVISIONING | 空心圆 | `...` | 等待手机连接热点... |
| CONNECTING | 实心圆 | `...` | 正在连接 AI... |
| ONLINE_IDLE | 实心圆 | — | AI 待命 |
| LISTENING | 实心圆 + 脉冲外环 | — | 聆听中... |
| SPEAKING | 3 条音柱 | — | 说话中... |
| UPGRADING | 实心圆 | `↑` | 固件升级中... |
| ERROR | 空心圆 | `!` | AI 暂不可用 · 长按 USER 查看 |

`SetChatMessage` / `ClearChatMessages` 也走 `SetAiBarTextAll`，与状态文案分层管理（状态文案是基线，对话文案是临时覆盖）。

#### P0-2 WiFi 配网 3 步引导

`ShowWifiProvisioningQr` 重写：顶部 AI 卡处于 `PROVISIONING` 态；左半 3 步骤时间线（黑底白字序号圆 + 中文说明：① 用手机连接热点 ② 扫描右侧二维码 ③ 选择 WiFi 输入密码）；右半 144×144 QR；底部 `热点: <SSID>` / `配网页: <URL>`。

附 `scripts/fix_mojibake.py`，修复了 3 处双重 UTF-8 编码的中文乱码（`热点:` / `配网页:` / `二维码内存不足`）。

#### P0-3 听 / 说反馈 + BOOT 提示

- `PulseAnimTimerCb`：聆听时 bot icon 外环放大 20→32 px 并淡出（80 ms 帧、12 帧周期）。
- `SpeakAnimTimerCb`：说话时 icon 区替换为 3 条音柱，150 ms 切换 6 种高度模式。
- Clock 页底部加「按 BOOT 说话 · 单按 USER 切换页面」，用户首次按 BOOT 时调 `DismissBootHint()` 永久隐藏。

### 4.2 P1 信息架构

| ID | 改动 |
|---|---|
| P1-1 | 6 桌面页码圆点 (`y=36`，当前页实心、其余空心边)；USER 长按从 `ShowSystemInfo` 改成 `CycleDisplayModeReverse` (反向翻页) |
| P1-2 | 已确认 `AUTO_HOME_TIMEOUT_MS` (60 s) 已被 `DataUpdateTask` 716–722 使用；非番茄钟运行中自动回时钟。无需新代码。 |
| P1-3 | 格言空状态由英文样例 "Fall seven times..." 改为「尚未设置格言 / 双击 USER 刷新」，字号 / 透明度同步降到次要级别；`UpdateQuoteText` 收到真实内容时切回正常样式。 |

### 4.3 P2 视觉一致性

| ID | 改动 |
|---|---|
| P2-2 | 已确认充电图标切换在 `data_update_task.cc` 460–472 已实现 (`icon_mode==3 → ui_img_battery_charging`)。 |
| P2-3 | Pomodoro 中文统一："FOCUS ON THE NOW" → "专注此刻"，"Start Focus" → "开始专注"。 |
| P2-4 | Photo `<` `>` 改回 24 px 实色 (`LV_OPA_COVER`)，状态 "1 / 1" 保持 14 px / 0.55 opacity。箭头表达"动作"，状态表达"信息"。 |

### 4.4 P3 锦上添花

| ID | 改动 |
|---|---|
| P3-1 | 状态栏右侧 10×10 月牙图标（实心圆 + 背景色咬口），`power_saving_` 为 true 时显示。 |
| P3-3 | 通过新增设置 modal 已能查到 LAN IP / OTA / 协议 (WiFi QR 页本身仍是配网模式专用，仍用热点 IP 才是正确语义)。 |
| P3-4 | BOOT 长按弹出 `settings_overlay_` 全屏 modal：标题黑底白字 + 状态 / IP / AI 后端 / OTA 信息 + 操作提示。 |
| P3-5 | `PlayPageTransition` 黑色幕布从 `y=-300` 下落 150 ms 至 `y=0`，再上滑 150 ms 至 `y=-300`，每次 `ApplyDisplayMode` 调用一次。 |

### 4.5 已主动 cancel 项

**P3-2 1-bit 几何 emoji 回退集** —— 评估后认为收益有限：现有 `current_theme_->emoji_collection()` + Chinese label fallback 已能在视觉上传达情绪；纯几何 emoji 需新增 ~150 行 + 单独的 1-bit 资产或绘图代码。文档化为后续可独立成 PR 的小项。

## 5. 按键映射的语义重排

| 控件 | 操作 | 改前 | 改后 |
|---|---|---|---|
| BOOT | 单击 | AI toggle / 配网 | AI toggle + `DismissBootHint` |
| BOOT | 长按 | 无 | **设置 / 关于 modal** (P3-4) |
| USER | 单击 | 切换页面 | 切换页面（同前） |
| USER | 双击 | 番茄钟 / 相册 / 刷新 | 同前 |
| USER | 长按 | `ShowSystemInfo` 滚屏 | **反向翻页** (P1-1) |

`ShowSystemInfo` 滚屏的功能没有丢失，只是从滚屏迁移到了设置 modal 的结构化展示。

## 6. 新增 / 修改文件清单

```
docs/UX_REFACTOR_2026-05.md                                  (本文档)
scripts/fix_mojibake.py                                      (新增, 31 行)

src/boards/waveshare-s3-rlcd-4.2/custom_lcd_display.h        (+94)
src/boards/waveshare-s3-rlcd-4.2/custom_lcd_display.cc       (+722 / -148)
src/boards/waveshare-s3-rlcd-4.2/data_update_task.cc         (+24 / -1)
src/boards/waveshare-s3-rlcd-4.2/music_ui.cc                 (+8 / -75)
src/boards/waveshare-s3-rlcd-4.2/pomodoro_ui.cc              (+11 / -45)
src/boards/waveshare-s3-rlcd-4.2/weather_ui.cc               (+5 / -39)
src/boards/waveshare-s3-rlcd-4.2/waveshare-s3-rlcd-4.2.cc    (+15 / -7)
```

## 7. 编译

```bash
export IDF_PATH=/opt/esp/esp-idf
source /opt/esp/esp-idf/export.sh
idf.py build
```

`ESP32RLCD42.bin` 4.74 MB，占用应用分区 91%。本次涉及的源文件全部 0 warning（除 2 条预先存在的 `-Wmissing-field-initializers`）。

### Cloud VM 环境必要的 managed_components 修复

记录在 `AGENTS.md` 之外的 1 条新增：

```bash
# zlib 组件缺少 zconf.h，从模板复制即可
cp managed_components/espressif__zlib/zlib/zconf.h.in \
   managed_components/espressif__zlib/zlib/zconf.h

# bmi270 组件缺 6.0 目录（已在 AGENTS.md 记录）
ln -s 5.5 managed_components/espressif__bmi270_sensor/6.0
```

## 8. 后续可独立成 PR 的小项

1. **P3-2 1-bit 几何 emoji 集**：8 种最常用情绪 (happy / sad / thinking / sleepy / surprised / loving / angry / neutral) 的纯图元绘制版本。
2. **全文 mojibake 修复**：约 20+ 处既有中文字符串是双重 UTF-8 编码（在屏上显示乱码）。`scripts/fix_mojibake.py` 已具备扩展能力，添加更多 (bad, good) 对即可。
3. **设置页内 USER 单击 = 重新配网**：当前框架已铺好（`ToggleSettingsOverlay` + `EnterWifiConfigMode`），需在 `waveshare-s3-rlcd-4.2.cc` 的 USER 单击 handler 加 5 行判断「overlay 是否可见」即可生效。
4. **接入 Coze 后端**：方案 B（新增 `CozeProtocol : public Protocol`）或方案 A（自建 Xiaozhi↔Coze 网关）。详见 PR #3 中的可行性分析。

## 9. 设计原则备忘

- **8 态 AI 卡的信号分级**：实心 vs 空心 = 在线/离线；动画 vs 静态 = 主动 vs 被动；角标 = 异常态注意力。
- **1-bit 屏的"层级"由"形态"而非"灰度"表达**：粗 vs 细字号、实色 vs 边框、有动画 vs 无动画。
- **首次开机的"无声教学"**：屏幕始终告诉用户下一步该做什么 (3 步引导 / BOOT 提示 / 空状态引导)，不依赖说明书。
- **隐藏手势谨慎使用**：USER 长按从 ShowSystemInfo 升级到反向翻页，因为前者是开发者向、后者是用户向。BOOT 长按这个用户不太可能误触的位置承担"设置"。
