# Hi3519AV100 硬件测试指南

## 测试准备

### 1. 硬件连接

```
[Hi3519AV100 开发板]
        │
        ├── HDMI ──────→ [显示器/电视] (1920×1080)
        │
        ├── USB ───────→ [鼠标] (必须)
        │
        ├── USB ───────→ [键盘] (可选)
        │
        └── MIPI CSI ──→ [内窥镜摄像头]
        
[串口/Telnet 用于调试]
```

### 2. 网络配置

确保开发板可以通过网络访问，用于部署：

```bash
# 在开发板上设置IP
ifconfig eth0 192.168.1.100 netmask 255.255.255.0

# 或检查当前IP
ifconfig
```

### 3. 创建必要目录

```bash
# 在开发板上执行
mkdir -p /opt/endoscope/record
mkdir -p /opt/endoscope/snapshot
chmod 777 /opt/endoscope/record
chmod 777 /opt/endoscope/snapshot
```

## 测试执行

### 步骤 1: 编译测试程序

```bash
cd /home/ydy/lv_port_pc_vscode/endoscope_hi3519

# 构建主程序
./build.sh -j4

# 构建测试程序
cd tests
make
```

### 步骤 2: 部署到开发板

```bash
# 方式1: 使用部署脚本
./deploy.sh

# 方式2: 手动复制
scp bin/endoscope_ui root@192.168.1.100:/opt/endoscope/
scp tests/hw_test root@192.168.1.100:/opt/endoscope/
```

### 步骤 3: 运行测试

```bash
# 通过串口或SSH登录开发板
telnet 192.168.1.100
# 或串口: screen /dev/ttyUSB0 115200

# 进入应用目录
cd /opt/endoscope

# 设置库路径
export LD_LIBRARY_PATH=/opt/endoscope/lib:$LD_LIBRARY_PATH

# 运行完整测试
./hw_test

# 或运行特定测试
./hw_test --display    # 仅测试显示
./hw_test --input      # 仅测试输入
./hw_test --video      # 仅测试视频
./hw_test --record     # 仅测试录制
```

## 测试项目详解

### TEST 1: 显示测试 (Display Test)

**验证内容：**
- HiFB G3 层初始化
- HDMI 1920×1080 输出
- ARGB8888 颜色格式
- LVGL 渲染

**预期结果：**
- HDMI 显示器显示红、绿、蓝三个色块
- 中央显示 "Hi3519AV100 Hardware Test" 文字
- 持续 5 秒

**故障排除：**
```bash
# 检查 HiFB 设备
ls -la /dev/fb*

# 检查 HDMI 输出
cat /sys/class/drm/card0-HDMI-A-1/status

# 检查 G3 层状态
cat /proc/hifb
```

### TEST 2: 输入测试 (Input Test)

**验证内容：**
- USB 鼠标检测 (evdev)
- 鼠标移动追踪
- 坐标映射 (1920×1080)

**预期结果：**
- 移动鼠标时终端打印坐标
- 鼠标按钮可以被检测到

**故障排除：**
```bash
# 检查输入设备
ls -la /dev/input/event*

# 测试原始输入
cat /dev/input/event0 | xxd

# 检查鼠标设备
ls -la /dev/input/mice
```

### TEST 3: 视频测试 (Video Test)

**验证内容：**
- MPP 系统初始化
- VI (摄像头) 采集
- VO (HDMI) 输出
- VI-VO 绑定

**预期结果：**
- HDMI 显示摄像头实时画面
- UI 菜单叠加在视频上方
- 持续 10 秒

**故障排除：**
```bash
# 检查 MPP 模块
lsmod | grep hi

# 检查摄像头节点
ls -la /dev/vi*

# 查看 MPP 日志
cat /var/log/mpp.log

# 检查视频格式
v4l2-ctl --all
```

### TEST 4: 录制测试 (Recording Test)

**验证内容：**
- VENC 初始化
- H.264 编码
- JPEG 抓拍
- 文件存储

**预期结果：**
- 5 秒 H.264 录像保存到 /opt/endoscope/record/
- JPEG 照片保存到 /opt/endoscope/snapshot/

**验证文件：**
```bash
# 检查录制文件
ls -lh /opt/endoscope/record/
# 应看到: 20240115_143022.h264

# 检查照片文件
ls -lh /opt/endoscope/snapshot/
# 应看到: 20240115_143027.jpg

# 播放录制文件 (在PC上)
ffplay -f h264 -i 20240115_143022.h264
```

## 常见问题解决

### 问题 1: HDMI 无输出

```bash
# 检查 HDMI 状态
cat /sys/kernel/debug/dri/0/HDMI-A-1/status

# 重新初始化 HDMI
himm 0x11200000 0x00000001  # 示例寄存器操作

# 检查分辨率设置
fbset
```

### 问题 2: 鼠标无响应

```bash
# 检查 USB 设备
lsusb

# 检查输入设备权限
chmod 666 /dev/input/event*

# 查看内核日志
dmesg | grep -i input
```

### 问题 3: 摄像头无画面

```bash
# 检查 MIPI CSI 连接
i2cdetect -y 0

# 检查摄像头驱动
lsmod | grep ov

# 重置 MPP
./sample_mpp_reset.sh
```

### 问题 4: 录制失败

```bash
# 检查目录权限
ls -la /opt/endoscope/

# 检查磁盘空间
df -h

# 检查 VENC 通道
cat /proc/venc
```

## 性能测试

### 帧率测试

```bash
# 在后台运行测试
./hw_test --video &

# 查看帧率统计
cat /proc/vo/vodev0
```

### 延迟测试

```bash
# 使用摄像头对准秒表
# 对比显示器时间与实际秒表
# 正常延迟应 < 100ms
```

## 验收标准

| 测试项 | 通过标准 | 优先级 |
|--------|----------|--------|
| HDMI 显示 | 显示测试图案，无花屏 | P0 |
| 鼠标输入 | 光标跟随鼠标移动 | P0 |
| 视频采集 | 摄像头画面清晰，无丢帧 | P0 |
| UI 叠加 | 菜单透明叠加正确 | P0 |
| 视频录制 | 生成可播放的 H.264 文件 | P1 |
| 拍照功能 | 生成有效的 JPEG 文件 | P1 |
| 长时间稳定性 | 连续运行 1 小时无崩溃 | P1 |

## 测试报告模板

```markdown
## Hi3519AV100 硬件测试报告

### 测试环境
- 开发板版本: 
- 内核版本: 
- SDK 版本: 
- 摄像头型号: 

### 测试结果

| 测试项 | 结果 | 备注 |
|--------|------|------|
| HDMI 显示 | □ 通过 □ 失败 | |
| 鼠标输入 | □ 通过 □ 失败 | |
| 视频采集 | □ 通过 □ 失败 | |
| UI 叠加 | □ 通过 □ 失败 | |
| 视频录制 | □ 通过 □ 失败 | |
| 拍照功能 | □ 通过 □ 失败 | |

### 问题记录
1. 
2. 

### 测试人员
- 日期: 
- 签名: 
```

## 下一步

测试全部通过后，系统即可投入使用。如需进一步优化：

1. 调整视频编码参数（码率、帧率）
2. 优化 UI 响应速度
3. 添加更多功能（冻结、缩放、测量）
