#!/bin/bash
# deploy_to_target.sh - 将内窥镜 UI 和字体部署到 Hi3519AV100 目标板
#
# 用法:
#   ./deploy_to_target.sh [TARGET_IP] [TARGET_USER] [FONT_DIR]
#   ./deploy_to_target.sh 192.168.1.10 root ../lang/fonts

TARGET_IP="${1:-192.168.1.10}"
TARGET_USER="${2:-root}"
FONT_SRC_DIR="${3:-../lang/fonts}"
TARGET_PATH="/opt/endoscope"

echo "========================================="
echo "Deploying Endoscope UI to Target Board"
echo "Target: ${TARGET_USER}@${TARGET_IP}"
echo "========================================="

if [ ! -f "bin/endoscope_ui" ]; then
    echo "Error: bin/endoscope_ui not found. Please build first with 'make'"
    exit 1
fi

echo "Creating directory structure on target..."
ssh ${TARGET_USER}@${TARGET_IP} "mkdir -p ${TARGET_PATH}/fonts ${TARGET_PATH}/images ${TARGET_PATH}/config" || {
    echo "Error: Failed to create directories on target"
    exit 1
}

echo "Copying binary..."
scp bin/endoscope_ui ${TARGET_USER}@${TARGET_IP}:${TARGET_PATH}/ || {
    echo "Error: Failed to copy binary"
    exit 1
}

echo "Copying font files..."
if [ -d "${FONT_SRC_DIR}" ]; then
    for font_file in "${FONT_SRC_DIR}"/*.ttf "${FONT_SRC_DIR}"/*.otf; do
        if [ -f "$font_file" ]; then
            echo "  Copying $(basename "$font_file")..."
            scp "$font_file" ${TARGET_USER}@${TARGET_IP}:${TARGET_PATH}/fonts/ || {
                echo "  Warning: Failed to copy $(basename "$font_file")"
            }
        fi
    done
else
    echo "Warning: Font directory ${FONT_SRC_DIR} not found, skipping fonts"
fi

echo "Setting permissions..."
ssh ${TARGET_USER}@${TARGET_IP} "chmod +x ${TARGET_PATH}/endoscope_ui"

echo ""
echo "========================================="
echo "Deployment Complete!"
echo "========================================="
echo ""
echo "To run on target board:"
echo "  cd ${TARGET_PATH} && ./endoscope_ui"
echo ""
echo "Expected files on target:"
ssh ${TARGET_USER}@${TARGET_IP} "ls -la ${TARGET_PATH}/" 2>/dev/null || echo "  (unable to list)"
echo ""
