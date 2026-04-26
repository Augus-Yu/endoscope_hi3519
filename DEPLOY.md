# Hi3519AV100 Endoscope UI Deployment Guide

## Quick Start

### 1. Build the Project
```bash
cd /home/ydy/lv_port_pc_vscode/endoscope_hi3519
make clean && make -j4
```

### 2. Deploy to Target Board
```bash
./deploy_to_target.sh [TARGET_IP] [TARGET_USER]
```

Default: `./deploy_to_target.sh 192.168.1.10 root`

### 3. Run on Target Board
SSH to the target board and run:
```bash
cd /opt/endoscope
./endoscope_ui
```

**⚠️ 重要：必须使用相对路径运行**

由于程序使用相对路径 `./fonts/` 加载字体文件，**必须在程序所在目录运行**：
- ✅ 正确：`cd /opt/endoscope && ./endoscope_ui`
- ❌ 错误：`/opt/endoscope/endoscope_ui` （会找不到字体）

如果需要从其他目录运行，请先切换到程序目录：
```bash
cd /opt/endoscope && ./endoscope_ui
```

## What Gets Deployed

### Binary
- `bin/endoscope_ui` → `/opt/endoscope/endoscope_ui`

### Font Files (Required for Chinese Display)
- `../lang/fonts/NotoSans-Regular.ttf` → `/opt/endoscope/fonts/NotoSans-Regular.ttf`
- `../lang/fonts/NotoSansCJKsc-Regular.otf` → `/opt/endoscope/fonts/NotoSansCJKsc-Regular.otf`

## Manual Deployment (if script fails)

```bash
# Create directories
ssh root@192.168.1.10 "mkdir -p /opt/endoscope/fonts"

# Copy binary
scp bin/endoscope_ui root@192.168.1.10:/opt/endoscope/

# Copy fonts
scp ../lang/fonts/NotoSans-Regular.ttf root@192.168.1.10:/opt/endoscope/fonts/
scp ../lang/fonts/NotoSansCJKsc-Regular.otf root@192.168.1.10:/opt/endoscope/fonts/

# Set permissions
ssh root@192.168.1.10 "chmod +x /opt/endoscope/endoscope_ui"
```

## Expected Directory Structure on Target

```
/opt/endoscope/
├── endoscope_ui          # Main executable (11.9MB)
├── fonts/
│   ├── NotoSans-Regular.ttf      # Latin characters (~300KB)
│   └── NotoSansCJKsc-Regular.otf # Chinese characters (~20MB)
├── images/               # (for future use)
└── config/               # (for future use)
```

## Troubleshooting

### Chinese characters not displaying
- Check that font files exist: `ls -la /opt/endoscope/fonts/`
- Check font file sizes (NotoSansCJKsc-Regular.otf should be ~20MB)
- Check console output for font loading errors
- **确认在程序目录运行**：`cd /opt/endoscope && ./endoscope_ui`（不能直接用绝对路径运行）

### Permission denied
```bash
chmod +x /opt/endoscope/endoscope_ui
```

### Cannot find liblvgl.so
The binary is statically linked - no shared libraries needed.

### HiFB initialization failed
- Ensure MPP is loaded: `lsmod | grep hi_media`
- Check framebuffer exists: `ls -la /dev/fb0`
- Check HDMI connection and display

## Testing Chinese Display

After running the UI, verify:
1. Main menu shows Chinese text (e.g., "设置", "拍照", "录像")
2. No tofu characters (□) appear
3. All UI labels are readable

If Chinese is missing:
1. Check console for: `Font file not found: ./fonts/NotoSansCJKsc-Regular.otf`
2. **确认在程序目录运行**：`cd /opt/endoscope && ./endoscope_ui`
3. Verify font file was copied correctly: `ls -la ./fonts/`
4. Restart the application

## Build Options

### Debug Build
```bash
make clean
CFLAGS="-g -O0 -DDEBUG" make -j4
```

### Release Build
```bash
make clean
CFLAGS="-O2" make -j4
```

## Next Steps

After UI is working with Chinese display:
1. Video capture integration (VI → VO)
2. Image capture and storage
3. Recording functionality
4. Settings persistence
