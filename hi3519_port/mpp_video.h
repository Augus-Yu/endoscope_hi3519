/**
 * @file mpp_video.h
 * @brief Hi3519 MPP视频采集模块头文件
 * @details 传感器配置通过 sensor_config.h 抽象, 支持多传感器
 */

#ifndef __MPP_VIDEO_H__
#define __MPP_VIDEO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sample_comm.h"
#include "sensor_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/

/* MPP管道内部编号 (与传感器无关) */
#define MPP_VI_DEV              3
#define MPP_VI_PIPE             0
#define MPP_VI_CHN              0
#define MPP_VPSS_GRP            0
#define MPP_VPSS_CHN            0
#define MPP_VO_DEV              0
#define MPP_VO_CHN              0
#define MPP_VENC_CHN            0

/* VO接口配置 */
#define VO_INTF_TYPE            VO_INTF_HDMI
#define VO_INTF_SYNC            VO_OUTPUT_1080P60

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
    video_state_t state;

    /* 传感器配置 (编译时选择) */
    const sensor_config_t *sensor;

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

HI_S32 mpp_system_init(HI_VOID);
HI_VOID mpp_system_exit(HI_VOID);
HI_S32 video_init(video_context_t *ctx);
HI_S32 video_start(video_context_t *ctx);
HI_VOID video_stop(video_context_t *ctx);
HI_VOID video_deinit(video_context_t *ctx);
video_state_t video_get_state(video_context_t *ctx);
HI_S32 video_set_position(HI_S32 x, HI_S32 y, HI_S32 width, HI_S32 height);
HI_S32 video_set_zoom(HI_S32 x, HI_S32 y, HI_S32 width, HI_S32 height);
video_context_t *video_get_context(HI_VOID);
HI_BOOL video_is_sensor_connected(HI_VOID);
HI_U32 video_get_fps(HI_VOID);

int mpp_video_init(void);
int mpp_video_start(void);
int mpp_video_stop(void);
int mpp_video_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __MPP_VIDEO_H__ */
