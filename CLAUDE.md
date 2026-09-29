# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 沟通语言

始终使用**中文**进行交流。

## 构建

这是 Hi3519AV100 嵌入式板端项目，使用 Makefile + 交叉编译，**不是** CMake 项目。

```bash
# 标准构建 (默认传感器 OV6946)
make -j4

# 切换传感器
make -j4 ACTIVE_SENSOR=OV9734

# 查看构建信息
make info

# 清理
make clean
```

交叉编译器：`/opt/hisi-linux/x86-arm/arm-himix200-linux/bin/arm-himix200-linux-gcc`（Makefile 中通过 `CROSS_COMPILE` 设置）。

产物：`bin/endoscope_ui`

## 项目架构

这是一个基于 LVGL v9.3 的**医疗电子内窥镜 UI** 嵌入式项目，运行在 Hi3519AV100 板端。与父目录的 PC 模拟器项目（CMake + SDL2）共享同一份 LVGL 源码 (`../lvgl/`)，但使用独立的 Makefile、`lv_conf.h`、硬件驱动层和 UI 代码。

### 核心区别 vs PC 模拟器

| | PC 模拟器 (父项目) | 嵌入式 (本项目) |
|---|---|---|
| 构建 | CMake | Makefile |
| 渲染 | SDL2 | Linux framebuffer (`/dev/fb0`) |
| 视频 | 无 | Hi3519 MPP 硬件管线 |
| 字体 | FreeType | Tiny TTF |
| 输入 | SDL 鼠标 | 触摸屏/鼠标 (evdev) |

### 源码结构

```
endoscope_hi3519/
├── main.c                     # 入口点：LVGL + MPP 初始化，主循环
├── Makefile                   # 交叉编译配置
├── lv_conf.h                  # LVGL 配置 (framebuffer、Tiny TTF、输入法等)
├── ui/                        # LVGL 界面层 (平台无关)
│   ├── endoscope_ui.c/.h      # 应用初始化、页面注册和切换
│   ├── endoscope_main.c/.h    # 主界面：预览窗口、变焦、LED、拍照录像
│   ├── endoscope_settings.c   # 系统设置 (含 FPN 校准)
│   ├── endoscope_image_settings.c  # 图像参数 (亮度/对比度/饱和度/锐度)
│   ├── endoscope_playback.c   # 回放网格浏览 (Flex 布局)
│   ├── endoscope_player.c     # 播放器 (视频逐帧 + 图片全屏浏览)
│   ├── endoscope_dialogs.c    # 通用对话框 (密码、确认)
│   ├── endoscope_splash.c     # 启动画面
│   ├── lang_manager.c/.h      # 中/英文多语言管理 (硬编码翻译表)
│   ├── font_manager.c/.h      # Tiny TTF 字体管理
│   ├── lv_pinyin_large_dict.c # 拼音大词库 (20924 汉字)
│   ├── screen_manager.c/.h    # 页面导航 (注册/切换/失效)
│   ├── ui_helpers.c/.h        # 日期时间等工具函数
│   └── config_manager.c/.h    # INI 风格配置文件读写
├── hi3519_port/               # Hi3519 硬件移植层 (平台相关)
│   ├── mpp_video.c/.h         # MPP 视频管线 (VI→VPSS→VO)
│   ├── sensor_config.c/.h     # 多传感器抽象 (编译时切换)
│   ├── mpp_record.c/.h        # H.264 录像 + JPEG 快照 (含 DCF 缩略图)
│   ├── mpp_playback.c/.h      # 视频回放 (VDEC 解码)
│   ├── mpp_fpn.c/.h           # FPN 自动校准 (暗帧采集 + 校正)
│   ├── lv_port_disp.c/.h      # LVGL 显示驱动 (/dev/fb0 + colorkey 透明)
│   └── lv_port_indev.c/.h     # LVGL 输入驱动 (鼠标/触摸)
└── hi3519_sdk/                # Hi3519 SDK 本地拷贝
    ├── include/               # MPP 头文件
    ├── lib/                   # MPP 静态库 / 动态库
    └── sample/common/         # SDK 示例源码 (sample_comm_*.c 编译进工程)
```

### 页面导航

所有页面通过 `screen_manager` 统一管理：每个页面对应一个 `endoscope_screen_t` 枚举值，通过 `screen_manager_register()` 注册 init/show/hide 回调，`screen_manager_navigate_to()` 切换页面。

### MPP 视频管线

```
OV6946 传感器 (400x400)
    ↓ VI (离线模式, VI_OFFLINE_VPSS_OFFLINE)
    ↓ VI 原始帧 → VB 池 (DDR)
    ↓
VPSS 组 (可销毁重建, 输出 400/600/800)
    ↓
VO 设备 → HDMI 1080p60 输出
    ↓
HIFB G0 (/dev/fb0) → LVGL UI + colorkey 透明叠加
```

关键设计：
- **离线模式**：VI 和 VPSS 通过 DDR 解耦，VPSS 可随时销毁重建来切换分辨率
- **colorkey 透明叠加**：LVGL 渲染到 fb0，视频区域写绿色 (0x00FF00) 作为 colorkey，VO 层在 fb0 下方透过 colorkey 显示
- **Ctrl+C 重启**：不退出 MPP 管线，进程重启时复用已有管道句柄
- **变焦**：解绑 → 销毁 VPSS 组 → 新分辨率重建 → 重绑

### 传感器配置

通过 `ACTIVE_SENSOR` Makefile 变量在编译时选择传感器（`ACTIVE_SENSOR_OV6946` / `ACTIVE_SENSOR_OV9734` 宏）。`sensor_config.c` 中定义各传感器的配置表（分辨率、VB 池大小、变焦级别等），新增传感器只需添加配置表和 `#ifdef` 分支。

### LVGL 配置要点

| 配置项 | 值 | 说明 |
|--------|-----|------|
| `LV_COLOR_DEPTH` | 32 | XRGB8888 |
| `LV_USE_LINUX_FBDEV` | 1 | Linux framebuffer 显示 |
| `LV_USE_SDL` | 0 | 禁用 SDL |
| `LV_USE_TINY_TTF` | 1 | Tiny TTF 字体渲染 |
| `LV_USE_FREETYPE` | 0 | 禁用 FreeType |
| `LV_USE_IME_PINYIN` | 1 | 拼音输入法 |
| `LV_IME_PINYIN_USE_DEFAULT_DICT` | 0 | 应用层替换大词库 |
| `LV_USE_FS_STDIO` | 1 | 标准 I/O 文件系统 |
| `LV_USE_BMP` | 1 | BMP 解码 (按钮图标) |
| `LV_USE_TJPGD` | 1 | JPEG 解码 |
| `LV_USE_LODEPNG` | 1 | PNG 解码 |
| `LV_USE_OS` | `LV_OS_PTHREAD` | pthread 多线程 |
| `LV_MEM_SIZE` | 8MB | 内存池 |
| `LV_TINY_TTF_CACHE_GLYPH_CNT` | 2048 | 字形缓存 |

编译宏 `LV_CONF_INCLUDE_SIMPLE` 和 `LV_LVGL_H_INCLUDE_SIMPLE` 已设置——LVGL 头文件直接通过 `#include "lvgl.h"` 引用，无需路径前缀。

### 多语言 (i18n)

字符串通过 `lang_manager.c` 管理，使用硬编码翻译表（无外部文件依赖）。支持中/英文运行时切换。`font_manager.c` 负责 Tiny TTF 字体加载，根据当前语言代码自动选择字体类型。

### LED 驱动

LM3630A LED 背光驱动，通过字符设备 `/dev/lm3630a` 控制，写入单字节 0~5 调节亮度。

## 部署

```bash
# 二进制
scp bin/endoscope_ui root@<ip>:/usr/bin/

# 字体 (必需)
scp -r lang/ root@<ip>:/usr/bin/

# 图标 (必需)
scp image/*.bmp root@<ip>:/usr/bin/image/
```

程序运行时工作目录为 `/usr/bin/`。板子需要的外部文件：`lang/fonts/` 下的 TTF/OTF 字体、`image/` 下的 BMP 按钮图标。录制的视频和照片保存在 `./endoscope/record/` 和 `./endoscope/snapshot/`。
