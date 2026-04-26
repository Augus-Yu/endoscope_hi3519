#!/bin/bash
#
# Remote Diagnostics Script for Hi3519AV100 Endoscope System
# Collects system information for troubleshooting
#

OUTPUT_DIR="/tmp/endoscope_diag_$(date +%Y%m%d_%H%M%S)"
OUTPUT_FILE="${OUTPUT_DIR}/diagnostics.txt"

echo "=========================================="
echo "Hi3519AV100 Endoscope Diagnostics"
echo "=========================================="
echo "Collecting system information..."
echo ""

mkdir -p ${OUTPUT_DIR}

echo "Endoscope System Diagnostics Report" > ${OUTPUT_FILE}
echo "Generated: $(date)" >> ${OUTPUT_FILE}
echo "==========================================" >> ${OUTPUT_FILE}
echo "" >> ${OUTPUT_FILE}

# System Information
echo "1. System Information" >> ${OUTPUT_FILE}
echo "--------------------" >> ${OUTPUT_FILE}
echo "Kernel: $(uname -a)" >> ${OUTPUT_FILE}
echo "Uptime: $(uptime)" >> ${OUTPUT_FILE}
echo "" >> ${OUTPUT_FILE}

# Memory Status
echo "2. Memory Status" >> ${OUTPUT_FILE}
echo "----------------" >> ${OUTPUT_FILE}
free -h >> ${OUTPUT_FILE}
echo "" >> ${OUTPUT_FILE}

# Disk Space
echo "3. Disk Space" >> ${OUTPUT_FILE}
echo "-------------" >> ${OUTPUT_FILE}
df -h >> ${OUTPUT_FILE}
echo "" >> ${OUTPUT_FILE}

# HiFB Status
echo "4. HiFB (Display) Status" >> ${OUTPUT_FILE}
echo "------------------------" >> ${OUTPUT_FILE}
if [ -d /proc/hifb ]; then
    ls -la /proc/hifb/ >> ${OUTPUT_FILE}
    for f in /proc/hifb/*; do
        echo "--- $f ---" >> ${OUTPUT_FILE}
        cat $f >> ${OUTPUT_FILE} 2>/dev/null
    done
else
    echo "HiFB proc not found" >> ${OUTPUT_FILE}
fi
echo "" >> ${OUTPUT_FILE}

# Framebuffer Devices
echo "5. Framebuffer Devices" >> ${OUTPUT_FILE}
echo "----------------------" >> ${OUTPUT_FILE}
ls -la /dev/fb* >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}
for fb in /dev/fb*; do
    if [ -e "$fb" ]; then
        echo "--- $fb Info ---" >> ${OUTPUT_FILE}
        fbset -fb $fb -i >> ${OUTPUT_FILE} 2>/dev/null
        echo "" >> ${OUTPUT_FILE}
    fi
done

# Input Devices
echo "6. Input Devices" >> ${OUTPUT_FILE}
echo "----------------" >> ${OUTPUT_FILE}
ls -la /dev/input/ >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}
cat /proc/bus/input/devices >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}

# MPP Modules
echo "7. MPP Kernel Modules" >> ${OUTPUT_FILE}
echo "---------------------" >> ${OUTPUT_FILE}
lsmod | grep -E "hi|mpi" >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}

# MPP Device Nodes
echo "8. MPP Device Nodes" >> ${OUTPUT_FILE}
echo "-------------------" >> ${OUTPUT_FILE}
ls -la /dev/vi* /dev/vo* /dev/venc* /dev/vdec* >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}

# HDMI Status
echo "9. HDMI Status" >> ${OUTPUT_FILE}
echo "--------------" >> ${OUTPUT_FILE}
if [ -f /sys/class/drm/card0-HDMI-A-1/status ]; then
    cat /sys/class/drm/card0-HDMI-A-1/status >> ${OUTPUT_FILE} 2>/dev/null
fi
if [ -d /proc/vo ]; then
    ls -la /proc/vo/ >> ${OUTPUT_FILE}
    for f in /proc/vo/*; do
        if [ -f "$f" ]; then
            echo "--- $f ---" >> ${OUTPUT_FILE}
            cat $f >> ${OUTPUT_FILE} 2>/dev/null
        fi
    done
fi
echo "" >> ${OUTPUT_FILE}

# Camera Detection
echo "10. Camera Detection" >> ${OUTPUT_FILE}
echo "--------------------" >> ${OUTPUT_FILE}
i2cdetect -y 0 >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}
ls -la /dev/video* >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}
dmesg | grep -i camera >> ${OUTPUT_FILE} 2>/dev/null
dmesg | grep -i mipi >> ${OUTPUT_FILE} 2>/dev/null
dmesg | grep -i ov >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}

# Process Status
echo "11. Running Processes" >> ${OUTPUT_FILE}
echo "---------------------" >> ${OUTPUT_FILE}
ps | grep -E "endoscope|hw_test|mpp" >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}

# Log Snippets
echo "12. Recent Kernel Messages" >> ${OUTPUT_FILE}
echo "--------------------------" >> ${OUTPUT_FILE}
dmesg | tail -50 >> ${OUTPUT_FILE}
echo "" >> ${OUTPUT_FILE}

# Application Logs (if exist)
echo "13. Application Logs" >> ${OUTPUT_FILE}
echo "--------------------" >> ${OUTPUT_FILE}
if [ -f /var/log/endoscope.log ]; then
    tail -100 /var/log/endoscope.log >> ${OUTPUT_FILE}
elif [ -f /tmp/endoscope.log ]; then
    tail -100 /tmp/endoscope.log >> ${OUTPUT_FILE}
else
    echo "No application logs found" >> ${OUTPUT_FILE}
fi
echo "" >> ${OUTPUT_FILE}

# Record Directory Status
echo "14. Record Directory Status" >> ${OUTPUT_FILE}
echo "---------------------------" >> ${OUTPUT_FILE}
ls -la /opt/endoscope/ >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}
ls -la /opt/endoscope/record/ >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}
ls -la /opt/endoscope/snapshot/ >> ${OUTPUT_FILE} 2>/dev/null
echo "" >> ${OUTPUT_FILE}

# Library Dependencies
echo "15. Library Dependencies" >> ${OUTPUT_FILE}
echo "------------------------" >> ${OUTPUT_FILE}
if [ -f /opt/endoscope/endoscope_ui ]; then
    echo "--- endoscope_ui ---" >> ${OUTPUT_FILE}
    ldd /opt/endoscope/endoscope_ui >> ${OUTPUT_FILE} 2>&1
fi
if [ -f /opt/endoscope/hw_test ]; then
    echo "--- hw_test ---" >> ${OUTPUT_FILE}
    ldd /opt/endoscope/hw_test >> ${OUTPUT_FILE} 2>&1
fi
echo "" >> ${OUTPUT_FILE}

# Create archive
echo "=========================================="
echo "Creating diagnostics archive..."
cd /tmp
tar -czf ${OUTPUT_DIR}.tar.gz $(basename ${OUTPUT_DIR})
echo ""
echo "Diagnostics collected successfully!"
echo ""
echo "Output: ${OUTPUT_DIR}.tar.gz"
echo ""
echo "To transfer to PC:"
echo "  scp root@<board_ip>:${OUTPUT_DIR}.tar.gz ./"
echo ""
echo "Quick Checks:"
echo "  - HiFB devices: $(ls /dev/fb* 2>/dev/null | wc -l) found"
echo "  - Input devices: $(ls /dev/input/event* 2>/dev/null | wc -l) found"
echo "  - MPP modules: $(lsmod | grep -c 'hi\\|mpi') loaded"
echo "  - Record dir: $(if [ -d /opt/endoscope/record ]; then echo 'OK'; else echo 'MISSING'; fi)"
echo ""
echo "For detailed report, see: ${OUTPUT_FILE}"
