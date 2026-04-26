#!/bin/bash
# 检查 Hi3519 环境

echo "=== Hi3519AV100 环境检查 ==="

# 1. 检查交叉编译器
if [ -f /opt/hisi-linux/x86-arm/arm-himix200-linux/bin/arm-himix200-linux-gcc ]; then
    echo "✓ 交叉编译器: 已找到"
    /opt/hisi-linux/x86-arm/arm-himix200-linux/bin/arm-himix200-linux-gcc --version | head -1
else
    echo "✗ 交叉编译器: 未找到"
fi

# 2. 检查 MPP 库
MPP_LIB="/home/ydy/Hi3519AV100_SDK_V2.0.2.0/smp/a53_linux/mpp/lib"
if [ -f "$MPP_LIB/libmpi.so" ]; then
    echo "✓ MPP 库: 已找到"
    ls -lh $MPP_LIB/libmpi.so | awk '{print "  ", $9, $5}'
else
    echo "✗ MPP 库: 未找到"
fi

# 3. 检查头文件
MPP_INC="/home/ydy/Hi3519AV100_SDK_V2.0.2.0/smp/a53_linux/mpp/include"
if [ -f "$MPP_INC/mpi_sys.h" ]; then
    echo "✓ MPP 头文件: 已找到"
else
    echo "✗ MPP 头文件: 未找到"
fi

# 4. 检查 LVGL
if [ -d "./lvgl" ]; then
    echo "✓ LVGL: 已找到"
else
    echo "✗ LVGL: 需要下载"
fi

echo ""
echo "=== 下一步 ==="
echo "1. 运行 ./setup.sh 初始化项目"
echo "2. 编译: cd build && make"
echo "3. 部署到开发板: ./deploy.sh"
