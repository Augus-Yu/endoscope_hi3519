/**
 * @file mpp_video.h
 * @brief Hi3519 MPP视频采集模块头文件
 * @details 负责OV6946传感器初始化、视频流采集和VO显示
 */

#ifndef __MPP_VIDEO_H__
#define __MPP_VIDEO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "/home/ydy/Hi3519AV100_SDK_V2.0.2.0/smp/a53_linux/mpp/sample/common/sample_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/* OV6946传感器配置 */
#define OV6946_VI_DEV           3
#define OV6946_VI_PIPE          0
#define OV6946_VI_CHN           0
#define OV6946_VPSS_GRP         0
#define OV6946_VPSS_CHN         0
#define OV6946_VO_DEV           0
#define OV6946_VO_CHN           0
#define OV6946_VENC_CHN         0

/* 视频分辨率 */
#define VIDEO_WIDTH             400
#define VIDEO_HEIGHT            400
#define VIDEO_FPS               30

/* VO视频输出配置 - 屏幕正中间显示 */
#define VO_INTF_TYPE            VO_INTF_HDMI
#define VO_INTF_SYNC            VO_OUTPUT_1080P60
#define VO_DISPLAY_X            760     /* (1920-400)/2 = 760，水平居中 */
#define VO_DISPLAY_Y            340     /* (1080-400)/2 = 340，垂直居中 */
#define VO_DISPLAY_WIDTH        400     /* OV6946标准宽度 */
#define VO_DISPLAY_HEIGHT       400     /* OV6946标准高度 */

/* 错误码 */
#define MPP_SUCCESS             0
#define MPP_FAILURE             -1

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    VIDEO_STATE_IDLE = 0,
    VIDEO_STATE_INIT,
    VIDEO_STATE_RUNNING,
    VIDEO_STATE_STOPPED,
    VIDEO_STATE_ERROR
} video_state_t;

typedef struct {
    /* 视频状态 */
    video_state_t state;
    
    /* 分辨率 */
    HI_U32 width;
    HI_U32 height;
    HI_U32 fps;
    
    /* 模块句柄 */
    VI_DEV vi_dev;
    VI_PIPE vi_pipe;
    VI_CHN vi_chn;
    VPSS_GRP vpss_grp;
    VPSS_CHN vpss_chn;
    VO_DEV vo_dev;
    VO_CHN vo_chn;
    VENC_CHN venc_chn;
    
    /* 配置 */
    SAMPLE_VI_CONFIG_S vi_config;
    SAMPLE_VO_CONFIG_S vo_config;
    
    /* 运行标志 */
    volatile HI_BOOL b_running;
    
    /* 统计 */
    HI_U32 frame_count;
    HI_U64 start_time;
} video_context_t;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化MPP系统
 * @return 0成功，其他失败
 */
HI_S32 mpp_system_init(HI_VOID);

/**
 * @brief 退出MPP系统
 */
HI_VOID mpp_system_exit(HI_VOID);

/**
 * @brief 初始化视频采集模块
 * @param ctx 视频上下文
 * @return 0成功，其他失败
 */
HI_S32 video_init(video_context_t *ctx);

/**
 * @brief 启动视频采集
 * @param ctx 视频上下文
 * @return 0成功，其他失败
 */
HI_S32 video_start(video_context_t *ctx);

/**
 * @brief 停止视频采集
 * @param ctx 视频上下文
 */
HI_VOID video_stop(video_context_t *ctx);

/**
 * @brief 反初始化视频模块
 * @param ctx 视频上下文
 */
HI_VOID video_deinit(video_context_t *ctx);

/**
 * @brief 获取视频状态
 * @param ctx 视频上下文
 * @return 视频状态
 */
video_state_t video_get_state(video_context_t *ctx);

/**
 * @brief 设置视频区域位置（VO图层在屏幕上的位置）
 * @param x 左上角X坐标
 * @param y 左上角Y坐标
 * @param width 宽度
 * @param height 高度
 * @return 0成功，其他失败
 */
HI_S32 video_set_position(HI_S32 x, HI_S32 y, HI_S32 width, HI_S32 height);

/**
 * @brief 获取全局视频上下文
 * @return 视频上下文指针
 */
video_context_t *video_get_context(HI_VOID);

/**
 * @brief 检查传感器是否连接
 * @return HI_TRUE已连接，HI_FALSE未连接
 */
HI_BOOL video_is_sensor_connected(HI_VOID);

/**
 * @brief 获取当前帧率
 * @return 帧率
 */
HI_U32 video_get_fps(HI_VOID);

/**
 * @brief 简化版视频初始化（自动创建上下文）
 * @return 0成功，其他失败
 */
int mpp_video_init(void);

/**
 * @brief 简化版视频启动
 * @return 0成功，其他失败
 */
int mpp_video_start(void);

/**
 * @brief 简化版视频停止
 * @return 0成功，其他失败
 */
int mpp_video_stop(void);

/**
 * @brief 简化版视频反初始化
 * @return 0成功，其他失败
 */
int mpp_video_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __MPP_VIDEO_H__ */
