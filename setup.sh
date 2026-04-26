#!/bin/bash
# 初始化 Hi3519 项目

set -e

echo "=== 初始化 Hi3519 内窥镜 UI 项目 ==="

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "项目根目录: $PROJECT_ROOT"

# 1. 创建目录结构
echo "创建目录结构..."
mkdir -p build
mkdir -p hi3519_port
mkdir -p endoscope_ui
mkdir -p fonts
mkdir -p lang

# 2. 复制 UI 源码
echo "复制 UI 源码..."
cp -r "$PROJECT_ROOT/main/src/endoscope_ui/"* endoscope_ui/ 2>/dev/null || echo "  警告: UI 源码复制失败，请手动复制"

# 3. 复制字体和语言文件
echo "复制资源文件..."
cp -r "$PROJECT_ROOT/lang/"* lang/ 2>/dev/null || echo "  警告: 语言文件复制失败"
cp -r "$PROJECT_ROOT/main/src/endoscope_ui/fonts/"* fonts/ 2>/dev/null || echo "  警告: 字体文件复制失败"

# 4. 下载 LVGL (如果本地没有)
if [ ! -d "lvgl" ]; then
    echo "下载 LVGL v8.3..."
    git clone --depth 1 --branch release/v8.3 https://github.com/lvgl/lvgl.git
    
    # 复制 lv_conf.h
    cp lvgl/lv_conf_template.h lv_conf.h
    # 启用配置文件
    sed -i 's/#if 0/#if 1/' lv_conf.h
fi

echo ""
echo "=== 项目初始化完成 ==="
echo "目录结构:"
tree -L 2 . 2>/dev/null || find . -maxdepth 2 -type d

echo ""
echo "下一步:"
echo "  make          # 编译项目"
echo "  make deploy   # 部署到开发板"
