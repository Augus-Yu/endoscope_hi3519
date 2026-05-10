/**
 * @file mpp_video.c
 * @brief Hi3519 MPP视频采集模块实现
 * @details OV6946传感器初始化、VI->VPSS->VO视频管道
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/fb.h>

#include "mpp_video.h"
#include "hifb.h"

/*********************
 *      DEFINES
 *********************/

#define VIDEO_BUFFER_COUNT      3
#define PIXEL_FORMAT            PIXEL_FORMAT_YVU_SEMIPLANAR_420
/**********************
 *  STATIC VARIABLES
  **********************/

static video_context_t g_video_ctx = {0};
static HI_BOOL s_mpp_initialized = HI_FALSE;

static RECT_S s_vo_display_rect = {
    .s32X = VO_DISPLAY_X,
    .s32Y = VO_DISPLAY_Y,
    .u32Width = VO_DISPLAY_WIDTH,
    .u32Height = VO_DISPLAY_HEIGHT
};

/**********************
 *  STATIC PROTOTYPES
 **********************/

static HI_S32 vi_init_ov6946(video_context_t *ctx);
static HI_S32 vpss_init(video_context_t *ctx);
static HI_S32 vo_init(video_context_t *ctx);
static HI_S32 bind_modules(video_context_t *ctx);
static HI_VOID vi_deinit(video_context_t *ctx);
static HI_VOID vpss_deinit(video_context_t *ctx);
static HI_VOID vo_deinit(video_context_t *ctx);
static HI_VOID unbind_modules(video_context_t *ctx);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

HI_S32 mpp_system_init(HI_VOID)
{
    HI_S32 s32Ret;
    VB_CONFIG_S stVbConf;
    HI_U32 u32BlkSize;
    PIC_SIZE_E enPicSize = PIC_400P;
    SIZE_S stSize;

    if (s_mpp_initialized) {
        printf("MPP system already initialized, skipping\n");
        return HI_SUCCESS;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret) {
        printf("Get picture size failed!\n");
        return s32Ret;
    }

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height,
                                          PIXEL_FORMAT, DATA_BITWIDTH_8,
                                          COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 20;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height,
                                       PIXEL_FORMAT_RGB_BAYER_16BPP,
                                       COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 15;

    s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf, VB_SUPPLEMENT_JPEG_MASK);
    if (HI_SUCCESS != s32Ret) {
        printf("MPP system init returned 0x%x, assuming already initialized\n", s32Ret);
        s_mpp_initialized = HI_TRUE;
        return HI_SUCCESS;
    }

    printf("MPP system initialized\n");
    return HI_SUCCESS;
}

HI_VOID mpp_system_exit(HI_VOID)
{
    SAMPLE_COMM_SYS_Exit();
    printf("MPP system exited\n");
}

HI_S32 video_init(video_context_t *ctx)
{
    HI_S32 s32Ret;

    if (ctx == NULL) {
        return MPP_FAILURE;
    }

    memset(ctx, 0, sizeof(video_context_t));
    ctx->state = VIDEO_STATE_IDLE;
    ctx->vi_dev = OV6946_VI_DEV;
    ctx->vi_pipe = OV6946_VI_PIPE;
    ctx->vi_chn = OV6946_VI_CHN;
    ctx->vpss_grp = OV6946_VPSS_GRP;
    ctx->vpss_chn = OV6946_VPSS_CHN;
    ctx->vo_dev = OV6946_VO_DEV;
    ctx->vo_chn = OV6946_VO_CHN;
    ctx->venc_chn = OV6946_VENC_CHN;
    ctx->width = VIDEO_WIDTH;
    ctx->height = VIDEO_HEIGHT;
    ctx->fps = VIDEO_FPS;

    s32Ret = mpp_system_init();
    if (HI_SUCCESS != s32Ret) {
        printf("MPP system init returned 0x%x, assuming already initialized\n", s32Ret);
    }

    /* If MPP was already running, skip VI/VPSS/VO init and assume
       everything is in the state from the previous session. */
    if (s_mpp_initialized) {
        ctx->state = VIDEO_STATE_INIT;
        printf("Video already initialized from previous session\n");
        return HI_SUCCESS;
    }

    s32Ret = vi_init_ov6946(ctx);
    if (HI_SUCCESS != s32Ret) {
        printf("VI init failed!\n");
        ctx->state = VIDEO_STATE_ERROR;
        return s32Ret;
    }

    s32Ret = vpss_init(ctx);
    if (HI_SUCCESS != s32Ret) {
        printf("VPSS init failed!\n");
        vi_deinit(ctx);
        ctx->state = VIDEO_STATE_ERROR;
        return s32Ret;
    }

    s32Ret = vo_init(ctx);
    if (HI_SUCCESS != s32Ret) {
        printf("VO init failed (may already be running): 0x%x\n", s32Ret);
        /* If VO is already running from a previous session, continue.
           This happens when video_deinit skips vo_deinit to keep fb0 alive. */
    }

    s32Ret = bind_modules(ctx);
    if (HI_SUCCESS != s32Ret) {
        printf("Bind modules failed: 0x%x!\n", s32Ret);
        vpss_deinit(ctx);
        vi_deinit(ctx);
        ctx->state = VIDEO_STATE_ERROR;
        return s32Ret;
    }

    ctx->state = VIDEO_STATE_INIT;
    printf("Video initialized\n");
    return HI_SUCCESS;
}

HI_S32 video_start(video_context_t *ctx)
{
    if (ctx == NULL || ctx->state != VIDEO_STATE_INIT) {
        return MPP_FAILURE;
    }

    ctx->b_running = HI_TRUE;
    ctx->start_time = 0;
    ctx->frame_count = 0;
    ctx->state = VIDEO_STATE_RUNNING;

    printf("Video started\n");
    return HI_SUCCESS;
}

HI_VOID video_stop(video_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->b_running = HI_FALSE;
    ctx->state = VIDEO_STATE_STOPPED;

    printf("Video stopped\n");
}

HI_VOID video_deinit(video_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    video_stop(ctx);
    unbind_modules(ctx);

    /* Disable VO channel to clear the last video frame from screen.
       Only the channel is disabled; the VO layer and device stay
       enabled so that /dev/fb0 (HIFB G0) remains accessible. */
    HI_MPI_VO_DisableChn(ctx->vo_dev, ctx->vo_chn);

    vpss_deinit(ctx);
    vi_deinit(ctx);
    /* Do NOT disable VO or exit MPP system.
       Hi3519 HIFB (/dev/fb0) is tightly coupled to the VO pipeline.
       Once HI_MPI_VO_Disable() is called, fb0 becomes permanently
       locked (EPERM) until the next system reboot.
       Keeping VO alive allows the UI to restart immediately. */

    ctx->state = VIDEO_STATE_IDLE;
    printf("Video deinitialized (VO/MPP kept alive for HIFB)\n");
}

video_state_t video_get_state(video_context_t *ctx)
{
    if (ctx == NULL) {
        return VIDEO_STATE_ERROR;
    }
    return ctx->state;
}

video_context_t *video_get_context(HI_VOID)
{
    return &g_video_ctx;
}

HI_BOOL video_is_sensor_connected(HI_VOID)
{
    /* Note: Hi3519 SDK doesn't have a direct sensor detection API.
     * The actual detection is done during VI initialization.
     * This function returns a placeholder value - the real check
     * happens when video_init() is called.
     * 
     * For a more robust implementation, you could:
     * 1. Try to access the sensor via I2C directly
     * 2. Check if the MIPI/CMOS interface detects a signal
     * 3. Use a GPIO pin connected to the sensor detect line
     */
    return HI_TRUE;
}

HI_U32 video_get_fps(HI_VOID)
{
    video_context_t *ctx = video_get_context();
    if (ctx == NULL || ctx->start_time == 0) {
        return 0;
    }

    return ctx->fps;
}

HI_S32 video_set_position(HI_S32 x, HI_S32 y, HI_S32 width, HI_S32 height)
{
    s_vo_display_rect.s32X = x;
    s_vo_display_rect.s32Y = y;
    s_vo_display_rect.u32Width = width;
    s_vo_display_rect.u32Height = height;

    /* 运行时更新 VO 显示位置 */
    video_context_t *ctx = video_get_context();
    if (ctx && ctx->state >= VIDEO_STATE_INIT) {
        POINT_S stPos = {x, y};
        HI_MPI_VO_SetChnDisplayPosition(ctx->vo_dev, ctx->vo_chn, &stPos);
    }

    return HI_SUCCESS;
}

/**********************
 *   STATIC FUNCTIONS
  **********************/

static HI_S32 vi_init_ov6946(video_context_t *ctx)
{
    HI_S32 s32Ret;
    HI_S32 s32ViCnt = 1;
    HI_S32 s32WorkSnsId = 1;

    WDR_MODE_E enWDRMode = WDR_MODE_NONE;
    DYNAMIC_RANGE_E enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E enPixFormat = PIXEL_FORMAT;
    VIDEO_FORMAT_E enVideoFormat = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E enCompressMode = COMPRESS_MODE_NONE;

    SAMPLE_COMM_VI_GetSensorInfo(&ctx->vi_config);

    ctx->vi_config.s32WorkingViNum = s32ViCnt;
    ctx->vi_config.as32WorkingViId[0] = 1;
    ctx->vi_config.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev = ctx->vi_dev;
    ctx->vi_config.astViInfo[s32WorkSnsId].stDevInfo.ViDev = ctx->vi_dev;
    ctx->vi_config.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode = enWDRMode;
    ctx->vi_config.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode =
        VI_ONLINE_VPSS_ONLINE;
    ctx->vi_config.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0] = ctx->vi_pipe;
    ctx->vi_config.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1] = -1;
    ctx->vi_config.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2] = -1;
    ctx->vi_config.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3] = -1;
    ctx->vi_config.astViInfo[s32WorkSnsId].stChnInfo.ViChn = ctx->vi_chn;
    ctx->vi_config.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat = enPixFormat;
    ctx->vi_config.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange =
        enDynamicRange;
    ctx->vi_config.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat =
        enVideoFormat;
    ctx->vi_config.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode =
        enCompressMode;

    s32Ret = SAMPLE_COMM_VI_StartVi(&ctx->vi_config);
    if (HI_SUCCESS != s32Ret) {
        printf("Start VI failed: 0x%x!\n", s32Ret);
        return s32Ret;
    }

    printf("VI initialized\n");
    return HI_SUCCESS;
}

static HI_S32 vpss_init(video_context_t *ctx)
{
    HI_S32 s32Ret;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};

    DYNAMIC_RANGE_E enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E enPixFormat = PIXEL_FORMAT;
    VIDEO_FORMAT_E enVideoFormat = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E enCompressMode = COMPRESS_MODE_NONE;

    memset(&stVpssGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat = enPixFormat;
    stVpssGrpAttr.u32MaxW = ctx->width;
    stVpssGrpAttr.u32MaxH = ctx->height;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;

    astVpssChnAttr[ctx->vpss_chn].u32Width = ctx->width;
    astVpssChnAttr[ctx->vpss_chn].u32Height = ctx->height;
    astVpssChnAttr[ctx->vpss_chn].enChnMode = VPSS_CHN_MODE_USER;
    astVpssChnAttr[ctx->vpss_chn].enCompressMode = enCompressMode;
    astVpssChnAttr[ctx->vpss_chn].enDynamicRange = enDynamicRange;
    astVpssChnAttr[ctx->vpss_chn].enVideoFormat = enVideoFormat;
    astVpssChnAttr[ctx->vpss_chn].enPixelFormat = enPixFormat;
    astVpssChnAttr[ctx->vpss_chn].stFrameRate.s32SrcFrameRate = ctx->fps;
    astVpssChnAttr[ctx->vpss_chn].stFrameRate.s32DstFrameRate = ctx->fps;
    astVpssChnAttr[ctx->vpss_chn].u32Depth = 0;
    astVpssChnAttr[ctx->vpss_chn].bMirror = HI_FALSE;
    astVpssChnAttr[ctx->vpss_chn].bFlip = HI_FALSE;
    astVpssChnAttr[ctx->vpss_chn].stAspectRatio.enMode = ASPECT_RATIO_NONE;

    abChnEnable[ctx->vpss_chn] = HI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_Start(ctx->vpss_grp, abChnEnable, &stVpssGrpAttr,
                                     astVpssChnAttr);
    if (HI_SUCCESS != s32Ret) {
        printf("Start VPSS failed: 0x%x!\n", s32Ret);
        return s32Ret;
    }

    printf("VPSS initialized\n");
    return HI_SUCCESS;
}

static HI_S32 vo_init(video_context_t *ctx)
{
    HI_S32 s32Ret;
    VO_PUB_ATTR_S stVoPubAttr = {0};
    VO_VIDEO_LAYER_ATTR_S stLayerAttr = {0};
    VO_DEV VoDev = ctx->vo_dev;
    VO_LAYER VoLayer = ctx->vo_dev;
    RECT_S stDefDispRect = {0, 0, 1920, 1080};

    s32Ret = SAMPLE_COMM_VO_GetDefConfig(&ctx->vo_config);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: GetDefConfig failed: 0x%x!\n", s32Ret);
        return s32Ret;
    }

    /* 步骤1: 启动VO设备 */
    stVoPubAttr.enIntfType = VO_INTF_TYPE;
    stVoPubAttr.enIntfSync = VO_INTF_SYNC;
    stVoPubAttr.u32BgColor = 0x00000000;

    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: StartDev(Dev=%d) failed: 0x%x!\n", VoDev, s32Ret);
        return s32Ret;
    }

    /* 步骤2: 启动VO图层 */
    s32Ret = SAMPLE_COMM_VO_GetWH(VO_INTF_SYNC,
                                   &stLayerAttr.stDispRect.u32Width,
                                   &stLayerAttr.stDispRect.u32Height,
                                   &stLayerAttr.u32DispFrmRt);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: GetWH failed: 0x%x!\n", s32Ret);
        goto cleanup_dev;
    }

    stLayerAttr.enPixFormat = ctx->vo_config.enPixFormat;
    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;

    if (0 != memcmp(&s_vo_display_rect, &stDefDispRect, sizeof(RECT_S))) {
        stLayerAttr.stDispRect = s_vo_display_rect;
    }

    stLayerAttr.stImageSize.u32Width = ctx->width;
    stLayerAttr.stImageSize.u32Height = ctx->height;

    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: StartLayer(Layer=%d) failed: 0x%x!\n", VoLayer, s32Ret);
        goto cleanup_dev;
    }

    /* 步骤3: 启动VO通道 */
    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, VO_MODE_1MUX);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: StartChn(Layer=%d, Mode=1MUX) failed: 0x%x!\n", VoLayer, s32Ret);
        goto cleanup_layer;
    }

    /* 步骤4: 启动HDMI输出 */
    s32Ret = SAMPLE_COMM_VO_HdmiStart(VO_INTF_SYNC);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: HdmiStart(Sync=%d) failed: 0x%x!\n", VO_INTF_SYNC, s32Ret);
        goto cleanup_chn;
    }

    printf("VO initialized at (%d,%d) %dx%d\n",
           s_vo_display_rect.s32X, s_vo_display_rect.s32Y,
           s_vo_display_rect.u32Width, s_vo_display_rect.u32Height);
    return HI_SUCCESS;

cleanup_chn:
    SAMPLE_COMM_VO_StopChn(VoLayer, VO_MODE_1MUX);
cleanup_layer:
    SAMPLE_COMM_VO_StopLayer(VoLayer);
cleanup_dev:
    SAMPLE_COMM_VO_StopDev(VoDev);
    return s32Ret;
}

static HI_S32 bind_modules(video_context_t *ctx)
{
    HI_S32 s32Ret;

    /* Unbind first in case they were left bound from a previous session */
    SAMPLE_COMM_VI_UnBind_VPSS(ctx->vi_pipe, ctx->vi_chn, ctx->vpss_grp);
    SAMPLE_COMM_VPSS_UnBind_VO(ctx->vpss_grp, ctx->vpss_chn,
                                ctx->vo_config.VoDev, ctx->vo_chn);

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ctx->vi_pipe, ctx->vi_chn, ctx->vpss_grp);
    if (HI_SUCCESS != s32Ret) {
        printf("VI bind VPSS failed: 0x%x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(ctx->vpss_grp, ctx->vpss_chn,
                                        ctx->vo_config.VoDev, ctx->vo_chn);
    if (HI_SUCCESS != s32Ret) {
        printf("VPSS bind VO failed: 0x%x!\n", s32Ret);
        SAMPLE_COMM_VI_UnBind_VPSS(ctx->vi_pipe, ctx->vi_chn, ctx->vpss_grp);
        return s32Ret;
    }

    printf("Modules bound\n");
    return HI_SUCCESS;
}

static HI_VOID vi_deinit(video_context_t *ctx)
{
    SAMPLE_COMM_VI_StopVi(&ctx->vi_config);
    printf("VI deinitialized\n");
}

static HI_VOID vpss_deinit(video_context_t *ctx)
{
    HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    abChnEnable[ctx->vpss_chn] = HI_TRUE;

    SAMPLE_COMM_VPSS_Stop(ctx->vpss_grp, abChnEnable);
    printf("VPSS deinitialized\n");
}

static HI_VOID vo_deinit(video_context_t *ctx)
{
    SAMPLE_COMM_VO_StopVO(&ctx->vo_config);
    printf("VO deinitialized\n");
}

static HI_VOID unbind_modules(video_context_t *ctx)
{
    SAMPLE_COMM_VPSS_UnBind_VO(ctx->vpss_grp, ctx->vpss_chn,
                                ctx->vo_config.VoDev, ctx->vo_chn);
    SAMPLE_COMM_VI_UnBind_VPSS(ctx->vi_pipe, ctx->vi_chn, ctx->vpss_grp);
    printf("Modules unbound\n");
}

/**********************
 *   SIMPLE API IMPL
 **********************/

int mpp_video_init(void)
{
    return video_init(video_get_context());
}

int mpp_video_start(void)
{
    return video_start(video_get_context());
}

int mpp_video_stop(void)
{
    video_stop(video_get_context());
    return 0;
}

int mpp_video_deinit(void)
{
    video_deinit(video_get_context());
    return 0;
}
