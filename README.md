# 医疗电子内窥镜 UI 系统

基于 LVGL v9.3 和 Hi3519AV100 的嵌入式医疗内窥镜控制界面。

## 功能

| 功能 | 说明 |
|------|------|
| 实时预览 | OV6946 传感器视频实时显示在 1920x1080 屏幕中央 |
| 电子变焦 | 三级变焦切换 (1x / 1.5x / 2x)，VPSS 动态重建实现 |
| 拍照 | 冻结帧 JPEG 快照，自动生成 DCF 缩略图 `_thm.jpg`，保存到 `./endoscope/snapshot/` |
| 录像 | H.264 编码，跟随当前变焦分辨率，保存到 `./endoscope/record/` |
| 回放 | 网格缩略图浏览，支持图片 (JPEG) 和视频 (H.264) 混合显示，逐帧播放 |
| 图片浏览 | 全屏查看 JPEG 图片，上一张/下一张循环导航 |
| 多语言 | 中文 / English 切换 |
| 图像设置 | 亮度、对比度、饱和度、锐度滑动调节 |
| 病人信息 | 拼音输入法录入 ID / 姓名 / 性别 / 年龄 |
| 密码管理 | 修改系统密码 |
| 冻结 | 暂停/恢复传感器输入，冻结状态下可拍照 |
| LED 亮度 | 头端 LED 6 档阶梯式调节 (0~5)，通过 LM3630A 驱动控制 |
| 输入法 | 拼音输入法，内置 20924 汉字大词库，覆盖生僻人名用字 |
| 外部字库 | Noto Sans CJK 字体 (16MB)，支持中文/日文/韩文显示 |
| 自定义图标 | 右侧按钮支持 BMP 图标替换 LV_SYMBOL，支持状态切换 |
| FPN 校准 | 自动暗帧 FPN (固定模式噪声) 校准，减少竖条纹 |

## 编译

```bash
cd endoscope_hi3519/
make -j4
```

交叉编译器路径：`/opt/hisi-linux/x86-arm/arm-himix200-linux/bin/`

产物：`bin/endoscope_ui`

## 部署到板子

程序运行时需要以下文件在板子上（以 `/mnt/` 为工作目录）：

```bash
# 二进制 (必需)
scp bin/endoscope_ui root@<ip>:/mnt/

# 字体文件 (必需, 否则中文显示方框)
scp -r lang/ root@<ip>:/mnt/

# 按钮图标 (必需, 否则右侧按钮无图标)
mkdir -p /mnt/image  # 在板子上执行
scp image/*.bmp root@<ip>:/mnt/image/
```

板子上的目录结构：
```
/mnt/
├── endoscope_ui          # 主程序
├── lang/
│   └── fonts/
│       ├── NotoSans-Regular.ttf
│       └── NotoSansCJKsc-Regular.otf
├── image/
│   ├── scene.bmp          # 设置
│   ├── wb.bmp             # 白平衡
│   ├── freeze.bmp         # 冻结
│   ├── freeze_active.bmp  # 冻结态
│   ├── capture.bmp        # 拍照
│   ├── record.bmp         # 录像
│   ├── recording.bmp      # 录像中
│   └── zoom.bmp           # 电子放大
├── endoscope/
│   ├── record/            # 录像文件 (.h264 + .meta)
│   └── snapshot/          # 拍照文件 (.jpg + _thm.jpg)
└── FPN_Frame_xxx.raw      # FPN 校准文件 (自动生成)
```

## 传感器切换

Makefile：

```makefile
ACTIVE_SENSOR ?= OV6946     # 默认 OV6946 (400x400)
# ACTIVE_SENSOR ?= OV9734   # 改为 OV9734 (1280x720)
```

新传感器只需在 `hi3519_port/sensor_config.c` 添加配置表，在 `sensor_config.h` 加 `#ifdef` 分支。配置表包含：分辨率、帧率、VB 池大小、变焦级别。

## 自定义按钮图标

主界面右侧 8 个按钮支持 BMP 格式自定义图标（80×80 像素，放在 `./image/` 目录）。

当前图标映射：

| 按钮 | BMP 文件 | 切换图 |
|------|----------|--------|
| 设置 | `scene.bmp` | — |
| 白平衡 | `wb.bmp` | — |
| 冻结 | `freeze.bmp` | `freeze_active.bmp` (冻结态) |
| 拍照 | `capture.bmp` | — |
| 录像 | `record.bmp` | `recording.bmp` (录像中) |
| 电子放大 | `zoom.bmp` | — |
| 病人信息录入 | (LV_SYMBOL 占位) | — |
| 回放 | (LV_SYMBOL 占位) | — |

BMP 格式无需额外解码器（`LV_USE_BMP=1` 原生支持）。可放入板子 `/mnt/image/` 目录。

## 字体与词库

### 外部字库

运行时从 `./lang/fonts/` 加载 TTF/OTF 字体文件：

| 文件 | 大小 | 用途 |
|------|------|------|
| NotoSans-Regular.ttf | 569 KB | 拉丁字母 |
| NotoSansCJKsc-Regular.otf | 16.4 MB | 中日韩汉字 |

使用 LVGL Tiny TTF 引擎实时渲染，字形缓存 2048 个。字体管理器 (`font_manager.c`) 根据语言代码自动选择字体类型。

### 拼音词库

内置大词库 (`ui/lv_pinyin_large_dict.c`)：4654 拼音条目，覆盖 20924 个 CJK 汉字。替换了 LVGL 默认的 323 条小词库。

## 操作说明

### 回放网格

- 主界面点击 **回放** 进入网格浏览
- 图片卡片显示缩略图（如有 `_thm.jpg`）或图标占位，视频卡片显示视频图标
- 点击卡片进入播放器：图片全屏浏览，视频逐帧播放
- 图片浏览支持上一张/下一张循环导航
- 文件列表自动扫描 `./endoscope/record/` (视频) 和 `./endoscope/snapshot/` (图片/缩略图)
- `_thm.jpg` 缩略图自动过滤，不会单独显示

### 拍照与缩略图

拍照时通过硬件 DCF (Design rule for Camera File system) 同时生成缩略图：

- 主 JPEG：`./endoscope/snapshot/YYYYMMDD_HHMMSS.jpg` (400×400)
- 缩略图：`./endoscope/snapshot/YYYYMMDD_HHMMSS_thm.jpg` (硬件生成)

回放网格自动检测 `_thm.jpg` 存在则显示缩略图，否则显示图标占位。

### 电子变焦

点击主界面右侧的 **电子放大** 按钮，在 1x → 1.5x → 2x 之间循环切换。视频窗口分辨率和显示区域同步变化。录制或回放时自动恢复到 1x。

### 录像

1. 调整变焦到目标级别
2. 点击 **录像** 按钮开始，再次点击停止（图标和文字切换为"录像中"）
3. 文件保存在 `./endoscope/record/`，包含 `.h264` 视频流和 `.meta` 分辨率信息
4. 回放时自动按录制分辨率解码显示

### FPN 校准

系统设置页提供 FPN 校准功能。校准前请确保镜体完全遮光：
1. 进入设置 → 系统 → FPN 校准
2. 确认遮光后点击确定
3. 系统自动切换 ONLINE 模式，关闭传感器曝光 + ISP 最低曝光 + 黑电平最大 + LED 关
4. 采集 16 帧暗帧数据，生成 FPN 校正文件 `./FPN_Frame_xxx.raw`
5. 冷启动时自动加载并启用 FPN 校正

### LED 亮度调节

主界面右下角滑动条，6 档阶梯式 (0~5)，拖动时实时写入 `/dev/lm3630a` 控制 LM3630A LED 驱动芯片。

## 工程结构

```
endoscope_hi3519/
├── main.c                            # 入口点 (LVGL 初始化、主循环)
├── Makefile                          # 交叉编译配置
├── lv_conf.h                         # LVGL 配置 (Tiny TTF、输入法、缓存、解码器等)
├── lang/fonts/                       # 外部字体文件
│   ├── NotoSans-Regular.ttf          # 拉丁字体
│   └── NotoSansCJKsc-Regular.otf     # CJK 字体
├── image/                            # 自定义按钮图标 (BMP, 80x80)
├── ui/                               # LVGL 界面
│   ├── endoscope_ui.c/.h             # 应用初始化、页面注册和切换
│   ├── endoscope_main.c/.h           # 主界面：预览窗口、按钮、变焦、LED、拍照
│   ├── endoscope_settings.c          # 设置页 (含 FPN 校准按钮)
│   ├── endoscope_image_settings.c    # 图像参数设置
│   ├── endoscope_playback.c          # 回放网格浏览 (Flex 布局、缩略图、延迟加载)
│   ├── endoscope_player.c            # 播放器 (视频播放 + 图片全屏浏览)
│   ├── endoscope_dialogs.c           # 通用对话框 (密码修改、确认弹窗)
│   ├── endoscope_splash.c            # 启动画面
│   ├── lang_manager.c/.h             # 中英文多语言管理 (硬编码翻译表)
│   ├── font_manager.c/.h             # TTF 字体管理器 (Tiny TTF 渲染)
│   ├── lv_pinyin_large_dict.c        # 拼音大词库 (20924 汉字)
│   ├── screen_manager.c/.h           # 页面导航管理
│   ├── ui_helpers.c/.h               # 日期时间等辅助函数
│   └── config_manager.c/.h           # INI 风格配置文件读写
├── hi3519_port/                      # Hi3519 硬件移植层
│   ├── mpp_video.c/.h                # MPP 视频管道 (VI→VPSS→VO)
│   ├── sensor_config.c/.h            # 多传感器抽象 (编译时切换)
│   ├── mpp_record.c/.h               # H.264 录像 + JPEG 快照 (含 DCF 缩略图)
│   ├── mpp_playback.c/.h             # 视频回放 (VDEC 解码)
│   ├── mpp_fpn.c/.h                  # FPN 自动校准 (暗帧采集 + 校正)
│   ├── lv_port_disp.c/.h             # LVGL 显示驱动 (/dev/fb0 + colorkey 透明)
│   └── lv_port_indev.c/.h            # LVGL 输入驱动 (鼠标/触摸)
└── hi3519_sdk/                       # Hi3519 SDK (本地拷贝)
    ├── include/                      # MPP 头文件
    ├── lib/                          # MPP 静态库 / 动态库
    └── sample/common/                # SDK 示例源码 (编译进本工程)
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
- **变焦实现**：解绑 → 销毁 VPSS 组 → 新分辨率重建 → 重绑 → 同步更新 VO 层和透明区域
- **透明叠加**：LVGL 渲染到 fb0，视频区域写绿色 (0x00FF00) 作为 colorkey，VO 层在 fb0 下方透过 colorkey 显示
- **DCF 缩略图**：拍照时 VENC 启用 `bSupportDcf=TRUE`，硬件编码 JPEG 内嵌缩略图，通过 `Getdcfinfo` 提取为 `_thm.jpg`
- **Ctrl+C 重启**：不退出 MPP 管线，重启时复用已有管道句柄

## LVGL 配置要点

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `LV_COLOR_DEPTH` | 32 | ARGB8888 显示 |
| `LV_USE_TJPGD` | 1 | Tiny JPEG 解码 (缩略图/图片浏览) |
| `LV_USE_BMP` | 1 | BMP 解码 (按钮图标) |
| `LV_USE_LODEPNG` | 0 | PNG 解码 (未启用，用 BMP 替代) |
| `LV_USE_FS_STDIO` | 1 | 标准 I/O 文件系统 (驱动字母 A) |
| `LV_FS_DEFAULT_DRIVER_LETTER` | 'A' | 默认 STDIO 驱动，路径无需前缀 |
| `LV_USE_FLEX` | 1 | Flex 布局 (回放网格) |
| `LV_USE_GRID` | 1 | Grid 布局 |
| `LV_TINY_TTF_CACHE_GLYPH_CNT` | 2048 | 字体缓存 |
| `LV_IME_PINYIN_USE_DEFAULT_DICT` | 0 | 使用自定义大词库 |

## LED 驱动

LM3630A LED 背光驱动 (`/dev/lm3630a`)，字符设备接口：

```bash
# 写入亮度等级 (0~5)
echo -n $'\x03' > /dev/lm3630a   # 3 档 (60%)
```
