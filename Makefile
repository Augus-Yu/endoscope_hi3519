# Hi3519AV100 Endoscope UI Makefile
# 参考SDK Makefile风格，简化编译规则

#===============================================================================
# 交叉编译器配置 (从SDK提取)
#===============================================================================
CROSS_COMPILE := arm-himix200-linux-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar
STRIP := $(CROSS_COMPILE)strip
SIZE := $(CROSS_COMPILE)size

#===============================================================================
# 路径配置
#===============================================================================
# MPP SDK路径（本地拷贝，无外部依赖）
HI3519_SDK_DIR := hi3519_sdk
MPP_LIB := $(HI3519_SDK_DIR)/lib
MPP_INC := $(HI3519_SDK_DIR)/include

# 项目路径
LVGL_DIR := ../lvgl
UI_DIR := ui
PORT_DIR := hi3519_port
OBJ_DIR := obj
BIN_DIR := bin

# 创建目录
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

#===============================================================================
# 源文件 - 使用wildcard自动收集
#===============================================================================
# LVGL核心源文件
LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name "*.c")

# LVGL演示代码
LVGL_DEMOS := $(shell find $(LVGL_DIR)/demos -name "*.c" 2>/dev/null)

# UI应用代码
UI_SRCS := $(wildcard $(UI_DIR)/*.c)

# Hi3519移植层代码
PORT_SRCS := $(wildcard $(PORT_DIR)/*.c)

# 主程序
MAIN_SRC := main.c

# SDK sample common 源码
SDK_SRCS := $(HI3519_SDK_DIR)/sample/common/sample_comm_vi.c \
            $(HI3519_SDK_DIR)/sample/common/sample_comm_vo.c \
            $(HI3519_SDK_DIR)/sample/common/sample_comm_vpss.c \
            $(HI3519_SDK_DIR)/sample/common/sample_comm_sys.c \
            $(HI3519_SDK_DIR)/sample/common/sample_comm_isp.c \
            $(HI3519_SDK_DIR)/sample/common/sample_comm_venc.c

# 所有源文件
ALL_SRCS := $(LVGL_SRCS) $(LVGL_DEMOS) $(UI_SRCS) $(PORT_SRCS) $(SDK_SRCS) $(MAIN_SRC)

#===============================================================================
# 对象文件 - 简单替换: .c -> .o 并加上obj前缀
#===============================================================================
# 将源文件路径转换为对象文件路径
# 例如: ../lvgl/src/core/lv_obj.c -> obj/lvgl/src/core/lv_obj.o
OBJS := $(ALL_SRCS:%.c=$(OBJ_DIR)/%.o)

# 去掉../前缀（用于LVGL路径）
OBJS := $(subst $(OBJ_DIR)/../,$(OBJ_DIR)/,$(OBJS))

#===============================================================================
# 头文件包含路径
#===============================================================================
INCS := -I.
INCS += -I$(LVGL_DIR)
INCS += -I$(LVGL_DIR)/src
INCS += -I$(MPP_INC)
INCS += -I$(UI_DIR)
INCS += -I$(PORT_DIR)
INCS += -I$(HI3519_SDK_DIR)/sample/common

#===============================================================================
# 编译标志 - 参考SDK设置
#===============================================================================
# Hi3519AV100特定标志（来自SDK Makefile.param）
ARCH_FLAGS := -mcpu=cortex-a53 -mfloat-abi=softfp -mfpu=neon-vfpv4

CFLAGS := $(ARCH_FLAGS)
CFLAGS += -Wall -O2 -g
CFLAGS += -fno-aggressive-loop-optimizations
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fstack-protector-strong -fPIC
CFLAGS += $(INCS)
CFLAGS += -DLV_CONF_INCLUDE_SIMPLE
CFLAGS += -DLV_LVGL_H_INCLUDE_SIMPLE
CFLAGS += -D_GNU_SOURCE
CFLAGS += -std=gnu99
# SDK sample common 传感器类型宏（用于编译 sample_comm_*.c）
CFLAGS += -DSENSOR0_TYPE=OV_OV9734_MIPI_1M_30FPS
CFLAGS += -DSENSOR1_TYPE=OV_OV6946_DC_1M_30FPS
CFLAGS += -DSENSOR2_TYPE=SONY_IMX290_SLAVE_MIPI_2M_60FPS_10BIT
CFLAGS += -DSENSOR3_TYPE=SONY_IMX290_SLAVE_MIPI_2M_60FPS_10BIT
CFLAGS += -DSENSOR4_TYPE=SONY_IMX334_MIPI_8M_30FPS_12BIT

# C++标志
CXXFLAGS := $(CFLAGS)
CXXFLAGS += -std=c++11

#===============================================================================
# 链接标志
#===============================================================================
LDFLAGS := $(ARCH_FLAGS)
LDFLAGS += -lpthread -lm -ldl -lstdc++
LDFLAGS += -Wl,-z,relro -Wl,-z,noexecstack -Wl,-z,now
LDFLAGS += -fno-aggressive-loop-optimizations

#===============================================================================
# MPP库
#===============================================================================
# 核心MPI库
MPI_LIBS := $(MPP_LIB)/libmpi.a
MPI_LIBS += $(MPP_LIB)/libhdmi.a
MPI_LIBS += $(MPP_LIB)/libtde.a
MPI_LIBS += $(MPP_LIB)/libdsp.a

# ISP相关库
ISP_LIBS := $(MPP_LIB)/libisp.a
ISP_LIBS += $(MPP_LIB)/lib_hiae.a
ISP_LIBS += $(MPP_LIB)/lib_hiawb.a
ISP_LIBS += $(MPP_LIB)/lib_hidehaze.a
ISP_LIBS += $(MPP_LIB)/lib_hidrc.a
ISP_LIBS += $(MPP_LIB)/lib_hildci.a

# 传感器库
SNS_LIBS := $(MPP_LIB)/libsns_ov9734.a \
            $(MPP_LIB)/libsns_ov6946.a \
            $(MPP_LIB)/libsns_imx290.a \
            $(MPP_LIB)/libsns_imx290_slave.a \
            $(MPP_LIB)/libsns_imx334.a

# 音频库
AUDIO_LIBS := $(MPP_LIB)/libVoiceEngine.a
AUDIO_LIBS += $(MPP_LIB)/libupvqe.a
AUDIO_LIBS += $(MPP_LIB)/libdnvqe.a

# 安全库
SECURE_LIBS := $(MPP_LIB)/libsecurec.a

# 所有库（用start-group/end-group解决依赖顺序）
ALL_LIBS := $(MPI_LIBS) $(ISP_LIBS) $(SNS_LIBS) $(AUDIO_LIBS) $(SECURE_LIBS)

#===============================================================================
# VPATH - 告诉make在哪里找源文件
#===============================================================================
VPATH := $(LVGL_DIR)/src:$(LVGL_DIR)/demos:$(UI_DIR):$(PORT_DIR):.
VPATH += $(shell find $(LVGL_DIR)/src -type d 2>/dev/null | tr '\n' ':')
VPATH += $(shell find $(LVGL_DIR)/demos -type d 2>/dev/null | tr '\n' ':')
VPATH += $(HI3519_SDK_DIR)/sample/common


#===============================================================================
# 编译规则
#===============================================================================
.PHONY: all clean info

TARGET := endoscope_ui

all: $(BIN_DIR)/$(TARGET)

# 链接目标
$(BIN_DIR)/$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ $^ \
		-Wl,--start-group $(ALL_LIBS) -Wl,--end-group
	@echo "Build complete: $@"
	$(SIZE) $@

# 编译规则: obj/lvgl/src/... -> 从各目录的.c文件编译
$(OBJ_DIR)/lvgl/src/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译规则: obj/lvgl/demos/... -> 从各目录的.c文件编译
$(OBJ_DIR)/lvgl/demos/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译规则: UI源码
$(OBJ_DIR)/$(UI_DIR)/%.o: $(UI_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译规则: Port源码
$(OBJ_DIR)/$(PORT_DIR)/%.o: $(PORT_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译规则: SDK sample common 源码
$(OBJ_DIR)/$(HI3519_SDK_DIR)/sample/common/%.o: $(HI3519_SDK_DIR)/sample/common/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# 编译规则: 主程序
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

#===============================================================================
# 清理
#===============================================================================
clean:
	@echo "Cleaning..."
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean complete."

#===============================================================================
# 信息输出
#===============================================================================
info:
	@echo "============================================================"
	@echo "Hi3519AV100 Endoscope UI Build Information"
	@echo "============================================================"
	@echo "Cross-compiler: $(CC)"
	@echo "C Compiler version:"
	@$(CC) --version 2>/dev/null | head -1 || echo "  (not available)"
	@echo ""
	@echo "Target: $(BIN_DIR)/$(TARGET)"
	@echo "Object directory: $(OBJ_DIR)"
	@echo ""
	@echo "MPP SDK: $(MPP_PATH)"
	@echo "MPP_INC: $(MPP_INC)"
	@echo "MPP_LIB: $(MPP_LIB)"
	@echo ""
	@echo "Source files:"
	@echo "  LVGL core: $(words $(LVGL_SRCS))"
	@echo "  LVGL demos: $(words $(LVGL_DEMOS))"
	@echo "  UI files: $(words $(UI_SRCS))"
	@echo "  Port files: $(words $(PORT_SRCS))"
	@echo "  Total objects: $(words $(OBJS))"
	@echo "============================================================"
