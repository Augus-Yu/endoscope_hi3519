# 医疗电子内窥镜 UI 系统

基于 LVGL v9.3 和 Hi3519AV100 的嵌入式医疗内窥镜控制界面。

## 功能

| 功能 | 说明 |
|------|------|
| 实时预览 | 传感器视频实时显示在 1920x1080 屏幕中央 |
| 电子变焦 | 三级变焦切换 (1x / 1.5x / 2x) |
| 拍照 | 冻结帧 JPEG 快照，保存到 ./endoscope/snapshot/ |
| 录像 | H.264 编码，跟随当前变焦分辨率，保存到 ./endoscope/record/ |
| 回放 | 读取 meta 文件自动匹配录制分辨率，支持逐帧播放 |
| 多语言 | 中文 / English 切换 |
| 图像设置 | 亮度、对比度、饱和度、锐度滑动调节 |
| 病人信息 | 录入和显示 ID / 姓名 / 性别 / 年龄 |
| 冻结 | 暂停/恢复传感器输入，冻结状态下可拍照 |

## 编译

```bash
cd endoscope_hi3519/
make clean && make -j4
```

交叉编译器路径：`/opt/hisi-linux/x86-arm/arm-himix200-linux/bin/`

产物：`bin/endoscope_ui`

## 传感器切换

Makefile 第 100 行：

```makefile
ACTIVE_SENSOR ?= OV6946     # 默认 OV6946 (400x400)
ACTIVE_SENSOR ?= OV9734     # 改为 OV9734 (1280x720)
```

新传感器只需在 `hi3519_port/sensor_config.c` 添加一张配置表，在 `sensor_config.h` 加一条 `#ifdef` 分支即可。配置表包含：分辨率、帧率、VB 池大小、变焦级别。

## 操作说明

### 电子变焦

点击主界面右侧的 **电子放大** 按钮，在 1x → 1.5x → 2x 之间循环切换。视频窗口分辨率和显示区域同步变化。进入回放或开始录像时自动恢复到 1x。

### 拍照

点击 **拍照** 按钮。冻结状态下从 VO 通道抓帧编码为 JPEG。非冻结状态通过 VENC 快照。图像保存到 `./endoscope/snapshot/`。

### 录像

1. 调整变焦到目标级别
2. 点击 **录像** 按钮开始，再次点击停止
3. 文件保存在 `./endoscope/record/` 目录下，每个录像有两个文件：
   - `.h264` — H.264 视频流
   - `.meta` — 录制分辨率信息（回放时自动读取）

计时器显示在左下角。

### 回放

- 主界面点击 **回放** 按钮进入回放列表
- 列出所有 `.h264` 录像文件
- 点击文件开始播放，自动匹配录制时的分辨率
- 播放器支持变速播放，显示进度

### 冻结

点击 **冻结** 按钮暂停传感器输入，画面停留在最后一帧。再次点击恢复。冻结状态下拍照无需等传感器新帧。

### 其他操作

- **病人信息录入** — 弹出对话框录入 ID / 姓名 / 性别 / 年龄
- **设置** — 进入设置页，切换语言、调整图像参数
- **白平衡** — 触发校准
- **LED 亮度** — 右侧面板底部滑动条调节

## 工程结构

```
endoscope_hi3519/
├── main.c                           # 入口点 (SDL/LVGL 初始化、主循环)
├── Makefile                         # 交叉编译配置 (arm-himix200-linux-gcc)
├── ui/                              # LVGL 界面
│   ├── endoscope_ui.c/.h            # 应用初始化、页面注册和切换
│   ├── endoscope_main.c/.h          # 主界面：预览窗口、按钮、变焦控制
│   ├── endoscope_settings.c         # 设置页
│   ├── endoscope_image_settings.c   # 图像参数设置 (亮度/对比度/饱和度/锐度)
│   ├── endoscope_playback.c         # 回放文件列表
│   ├── endoscope_player.c           # 播放器控制
│   ├── endoscope_dialogs.c          # 通用对话框
│   ├── endoscope_splash.c           # 启动画面
│   ├── lang_manager.c/.h            # 中英文多语言管理
│   ├── font_manager.c/.h            # 字体管理
│   ├── screen_manager.c/.h          # 页面导航管理
│   ├── ui_helpers.c/.h              # 日期时间等辅助函数
│   └── config_manager.c/.h          # INI 风格配置文件读写
├── hi3519_port/                     # Hi3519 硬件移植层
│   ├── mpp_video.c/.h               # MPP 视频管道 (VI→VPSS→VO)
│   ├── sensor_config.c/.h           # 多传感器抽象 (编译时切换)
│   ├── mpp_record.c/.h              # H.264 录像
│   ├── mpp_playback.c/.h            # 视频回放
│   ├── lv_port_disp.c/.h            # LVGL 显示驱动 (/dev/fb0 + colorkey 透明)
│   └── lv_port_indev.c/.h           # LVGL 输入驱动 (鼠标/触摸)
└── hi3519_sdk/                      # Hi3519 SDK (本地拷贝)
    ├── include/                     # MPP 头文件
    ├── lib/                         # MPP 静态库
    └── sample/common/               # SDK 示例源码 (编译进本工程)
```

## MPP 视频管道

```
OV6946 传感器 (400x400)
    ↓ VI_OFFLINE_VPSS_OFFLINE (离线模式)
    ↓ VI 原始帧 → VB 池 (DDR)
    ↓
VPSS 组 (可销毁重建, 输出 400/600/800)
    ↓
VO 设备 → HDMI 1080p60 输出
    ↓
HIFB G0 (/dev/fb0) → LVGL UI + colorkey 透明叠加
```

- **离线模式**：VI 和 VPSS 通过 DDR 解耦，VPSS 可随时销毁重建来切换分辨率
- **VB 池**：三个池（标准帧 / Bayer 数据 / 变焦大帧），按传感器分辨率动态配置
- **变焦实现**：解绑 → 销毁 VPSS 组 → 以新分辨率重建 → 重绑 → 同步更新 VO 层和透明区域
- **透明叠加**：LVGL 渲染到 fb0，视频区域写绿色 (0x00FF00) 作为 colorkey，VO 层在 fb0 下方透过 colorkey 显示
