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
static HI_BOOL s_mpp_init_called = HI_FALSE;
static HI_BOOL s_mpp_was_running = HI_FALSE;

static RECT_S s_vo_display_rect = {
    .s32X = VO_DISPLAY_X,
    .s32Y = VO_DISPLAY_Y,
    .u32Width = VO_DISPLAY_WIDTH,
    .u32Height = VO_DISPLAY_HEIGHT
};
static pthread_mutex_t s_vo_rect_mutex = PTHREAD_MUTEX_INITIALIZER;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static HI_S32 vi_init_ov6946(video_context_t *ctx);
static HI_S32 vpss_init(video_context_t *ctx);
static HI_S32 vo_init(video_context_t *ctx);
static HI_S32 bind_modules(video_context_t *ctx);
static HI_VOID vi_deinit(video_context_t *ctx);
static HI_VOID vpss_deinit(video_context_t *ctx);
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

    if (s_mpp_init_called) {
        printf("MPP system already initialized, skipping\n");
        return HI_SUCCESS;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret) {
        printf("Get picture size failed!\n");
        return s32Ret;
    }

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt = 3;

    /* Pool 0: 400x400 标准帧缓冲 */
    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height,
                                          PIXEL_FORMAT, DATA_BITWIDTH_8,
                                          COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 20;

    /* Pool 1: VI原始 Bayer 数据 */
    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height,
                                       PIXEL_FORMAT_RGB_BAYER_16BPP,
                                       COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 15;

    /* Pool 2: 变焦大帧 800x800 */
    u32BlkSize = COMMON_GetPicBufferSize(VIDEO_ZOOM_MAX_W, VIDEO_ZOOM_MAX_H,
                                          PIXEL_FORMAT, DATA_BITWIDTH_8,
                                          COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[2].u64BlkSize = u32BlkSize;
    stVbConf.astCommPool[2].u32BlkCnt = 8;

    s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf, VB_SUPPLEMENT_JPEG_MASK);
    if (HI_SUCCESS != s32Ret) {
        printf("MPP system init returned 0x%x, assuming already initialized\n", s32Ret);
        s_mpp_was_running = HI_TRUE;
        s_mpp_init_called = HI_TRUE;
        return HI_SUCCESS;
    }

    s_mpp_init_called = HI_TRUE;
    printf("MPP system initialized (VB pool: 400x400 + %dx%d zoom)\n",
           VIDEO_ZOOM_MAX_W, VIDEO_ZOOM_MAX_H);
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
    if (s_mpp_was_running) {
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
    pthread_mutex_lock(&s_vo_rect_mutex);
    s_vo_display_rect.s32X = x;
    s_vo_display_rect.s32Y = y;
    s_vo_display_rect.u32Width = width;
    s_vo_display_rect.u32Height = height;
    pthread_mutex_unlock(&s_vo_rect_mutex);
    return HI_SUCCESS;
}

/**
 * @brief 运行时电子放大 - 销毁重建VPSS组(模拟初始化)
 * @param level 缩放级别: 0=1x(400), 1=1.5x(600), 2=2x(800)
 * @details 运行时改VPSS分辨率无效, 只能销毁后重新CreateGrp.
 *          不退出MPP系统(保fb0), 只重建VPSS组和VO层.
 */
HI_S32 video_set_zoom(HI_S32 x, HI_S32 y, HI_S32 width, HI_S32 height)
{
    video_context_t *ctx = video_get_context();
    VO_LAYER VoLayer = ctx->vo_dev;
    HI_S32 s32Ret;

    if (ctx == NULL || ctx->state < VIDEO_STATE_RUNNING) {
        printf("video_set_zoom: video not running\n");
        return MPP_FAILURE;
    }

    printf("[ZOOM] === START: target=%dx%d pos=(%d,%d) ===\n",
           width, height, x, y);

    /* === 1. 读当前VPSS+VI状态 === */
    {
        VPSS_GRP_ATTR_S g;
        VPSS_CHN_ATTR_S c;
        HI_MPI_VPSS_GetGrpAttr(ctx->vpss_grp, &g);
        HI_MPI_VPSS_GetChnAttr(ctx->vpss_grp, ctx->vpss_chn, &c);
        printf("[ZOOM] VPSS before: GrpMax=%dx%d ChnOut=%dx%d\n",
               g.u32MaxW, g.u32MaxH, c.u32Width, c.u32Height);

        printf("[ZOOM] VI pipe=%d chn=%d dev=%d\n",
               ctx->vi_pipe, ctx->vi_chn, ctx->vi_dev);
    }

    /* === 2. 停VO === */
    printf("[ZOOM] Stop VO chn...\n");
    s32Ret = HI_MPI_VO_DisableChn(VoLayer, ctx->vo_chn);
    printf("[ZOOM] VO DisableChn ret=0x%x\n", s32Ret);
    s32Ret = HI_MPI_VO_DisableVideoLayer(VoLayer);
    printf("[ZOOM] VO DisableLayer ret=0x%x\n", s32Ret);

    /* === 3. 解绑 (离线模式: VI→VPSS也需解绑, VI不停) === */
    printf("[ZOOM] Unbind...\n");
    s32Ret = SAMPLE_COMM_VPSS_UnBind_VO(ctx->vpss_grp, ctx->vpss_chn,
                                ctx->vo_config.VoDev, ctx->vo_chn);
    printf("[ZOOM] Unbind VpssVo ret=0x%x\n", s32Ret);
    s32Ret = SAMPLE_COMM_VI_UnBind_VPSS(ctx->vi_pipe, ctx->vi_chn, ctx->vpss_grp);
    printf("[ZOOM] Unbind ViVpss ret=0x%x\n", s32Ret);

    /* === 4. 销毁VPSS === */
    printf("[ZOOM] Destroy VPSS grp%d...\n", ctx->vpss_grp);
    {
        HI_BOOL ab[VPSS_MAX_PHY_CHN_NUM] = {0};
        ab[ctx->vpss_chn] = HI_TRUE;
        s32Ret = SAMPLE_COMM_VPSS_Stop(ctx->vpss_grp, ab);
        printf("[ZOOM] VPSS Stop ret=0x%x\n", s32Ret);
    }

    /* === 5. 重建VPSS === */
    printf("[ZOOM] Create VPSS grp%d chn%d -> %dx%d...\n",
           ctx->vpss_grp, ctx->vpss_chn, width, height);
    {
        VPSS_GRP_ATTR_S g;
        VPSS_CHN_ATTR_S ac[VPSS_MAX_PHY_CHN_NUM];
        HI_BOOL ab[VPSS_MAX_PHY_CHN_NUM] = {0};

        memset(&g, 0, sizeof(g));
        g.stFrameRate.s32SrcFrameRate = -1;
        g.stFrameRate.s32DstFrameRate = -1;
        g.enDynamicRange = DYNAMIC_RANGE_SDR8;
        g.enPixelFormat = PIXEL_FORMAT;
        g.u32MaxW = (width  > 400) ? width  : 400;
        g.u32MaxH = (height > 400) ? height : 400;
        g.bNrEn = HI_TRUE;
        g.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
        g.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;

        memset(ac, 0, sizeof(ac));
        ac[ctx->vpss_chn].u32Width  = width;
        ac[ctx->vpss_chn].u32Height = height;
        ac[ctx->vpss_chn].enChnMode = VPSS_CHN_MODE_USER;
        ac[ctx->vpss_chn].enCompressMode = COMPRESS_MODE_NONE;
        ac[ctx->vpss_chn].enDynamicRange = DYNAMIC_RANGE_SDR8;
        ac[ctx->vpss_chn].enVideoFormat = VIDEO_FORMAT_LINEAR;
        ac[ctx->vpss_chn].enPixelFormat = PIXEL_FORMAT;
        ac[ctx->vpss_chn].stFrameRate.s32SrcFrameRate = ctx->fps;
        ac[ctx->vpss_chn].stFrameRate.s32DstFrameRate = ctx->fps;
        ac[ctx->vpss_chn].stAspectRatio.enMode = ASPECT_RATIO_NONE;

        ab[ctx->vpss_chn] = HI_TRUE;
        s32Ret = SAMPLE_COMM_VPSS_Start(ctx->vpss_grp, ab, &g, ac);
        printf("[ZOOM] VPSS Start(Create+StartGrp+SetChn+Enable) ret=0x%x\n", s32Ret);
        if (HI_SUCCESS != s32Ret) return s32Ret;
    }

    /* === 6. 读重建后VPSS状态 === */
    {
        VPSS_GRP_ATTR_S g;
        VPSS_CHN_ATTR_S c;
        HI_MPI_VPSS_GetGrpAttr(ctx->vpss_grp, &g);
        HI_MPI_VPSS_GetChnAttr(ctx->vpss_grp, ctx->vpss_chn, &c);
        printf("[ZOOM] VPSS after: GrpMax=%dx%d ChnOut=%dx%d\n",
               g.u32MaxW, g.u32MaxH, c.u32Width, c.u32Height);
    }

    /* === 7. 重绑 === */
    printf("[ZOOM] Rebind...\n");
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ctx->vi_pipe, ctx->vi_chn, ctx->vpss_grp);
    printf("[ZOOM] Bind ViVpss ret=0x%x\n", s32Ret);
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(ctx->vpss_grp, ctx->vpss_chn,
                              ctx->vo_config.VoDev, ctx->vo_chn);
    printf("[ZOOM] Bind VpssVo ret=0x%x\n", s32Ret);

    /* === 9. 重启VO === */
    printf("[ZOOM] Start VO %dx%d at (%d,%d)...\n", width, height, x, y);
    {
        VO_VIDEO_LAYER_ATTR_S la;
        s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoLayer, &la);
        printf("[ZOOM] VO GetLayerAttr ret=0x%x\n", s32Ret);
        la.stImageSize.u32Width  = width;
        la.stImageSize.u32Height = height;
        la.stDispRect.s32X = x;
        la.stDispRect.s32Y = y;
        la.stDispRect.u32Width  = width;
        la.stDispRect.u32Height = height;
        s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &la);
        printf("[ZOOM] VO StartLayer ret=0x%x\n", s32Ret);
        s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, VO_MODE_1MUX);
        printf("[ZOOM] VO StartChn ret=0x%x\n", s32Ret);
    }

    /* === 10. 最终VPSS状态 === */
    {
        VPSS_GRP_ATTR_S g;
        VPSS_CHN_ATTR_S c;
        HI_MPI_VPSS_GetGrpAttr(ctx->vpss_grp, &g);
        HI_MPI_VPSS_GetChnAttr(ctx->vpss_grp, ctx->vpss_chn, &c);
        printf("[ZOOM] VPSS final: GrpMax=%dx%d ChnOut=%dx%d\n",
               g.u32MaxW, g.u32MaxH, c.u32Width, c.u32Height);
    }

    video_set_position(x, y, width, height);
    printf("video_set_zoom: (%d,%d %dx%d) reinit done\n", x, y, width, height);
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
    /* 离线模式: VI和VPSS解耦, 帧经DDR传输, 支持运行时改VPSS分辨率 */
    ctx->vi_config.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode =
        VI_OFFLINE_VPSS_OFFLINE;
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
    SAMPLE_VO_CONFIG_S stVoConfig;

    s32Ret = SAMPLE_OV6946_COMM_VO_GetDefConfig(&stVoConfig);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: GetDefConfig failed: 0x%x!\n", s32Ret);
        return s32Ret;
    }

    stVoConfig.VoDev         = ctx->vo_dev;
    stVoConfig.enVoIntfType  = VO_INTF_TYPE;
    stVoConfig.enIntfSync    = VO_INTF_SYNC;
    stVoConfig.enPixFormat   = PIXEL_FORMAT;
    stVoConfig.u32BgColor    = 0x00000000;
    stVoConfig.u32DisBufLen  = 0;
    stVoConfig.enVoMode      = VO_MODE_1MUX;

    pthread_mutex_lock(&s_vo_rect_mutex);
    stVoConfig.stDispRect    = s_vo_display_rect;
    stVoConfig.stImageSize.u32Width  = s_vo_display_rect.u32Width;
    stVoConfig.stImageSize.u32Height = s_vo_display_rect.u32Height;
    pthread_mutex_unlock(&s_vo_rect_mutex);

    s32Ret = SAMPLE_OV6946_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret) {
        printf("VO: StartVO(OV6946) failed: 0x%x!\n", s32Ret);
        return s32Ret;
    }

    memcpy(&ctx->vo_config, &stVoConfig, sizeof(stVoConfig));

    printf("VO initialized at (%d,%d) %dx%d\n",
           s_vo_display_rect.s32X, s_vo_display_rect.s32Y,
           s_vo_display_rect.u32Width, s_vo_display_rect.u32Height);
    return HI_SUCCESS;
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
