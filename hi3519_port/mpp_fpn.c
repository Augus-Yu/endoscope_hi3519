/**
 * @file mpp_fpn.c
 * @brief FPN (Fixed Pattern Noise) 自动校准和校正
 */

#include "mpp_fpn.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* 内部函数声明 (sample_comm_vi.c 中定义但未导出) */
extern HI_S32 SAMPLE_VI_GetFrameBlkInfo(SAMPLE_VI_FRAME_CONFIG_S *pstFrmCfg,
    HI_S32 s32FrmCnt, SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);
extern HI_VOID SAMPLE_VI_COMM_ReleaseFrameBlkInfo(HI_S32 s32FrmCnt,
    SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);

/* FPN 校准阈值: 像素均值超过此值视为传感器未遮光 */
#define FPN_DARK_THRESHOLD      500
/* 暗帧检测: 抽样 1% 像素取均值 */
#define FPN_CHECK_SAMPLE_RATIO  100

static fpn_status_t g_fpn_status = FPN_STATUS_IDLE;

fpn_status_t mpp_fpn_get_status(void)
{
    return g_fpn_status;
}

static int check_dark_frame(SAMPLE_VI_FPN_CALIBRATE_INFO_S *info,
                             ISP_FPN_CALIBRATE_ATTR_S *attr)
{
    /* 简单检测: 校准完成后的 ISO 值不能过高 */
    if (attr->stFpnCaliFrame.u32Iso > FPN_DARK_THRESHOLD) {
        printf("[FPN] dark frame ISO=%u > threshold=%d, need retry\n",
               attr->stFpnCaliFrame.u32Iso, FPN_DARK_THRESHOLD);
        return 0; /* 不合格 */
    }
    printf("[FPN] dark frame ISO=%u OK\n", attr->stFpnCaliFrame.u32Iso);
    return 1; /* 合格 */
}

fpn_status_t mpp_fpn_calibrate(VI_PIPE ViPipe)
{
    SAMPLE_VI_FPN_CALIBRATE_INFO_S caliInfo;
    SAMPLE_VI_FPN_CORRECTION_INFO_S corrInfo;
    ISP_PUB_ATTR_S stPubAttr;
    SAMPLE_VI_FRAME_CONFIG_S stFrmCfg;
    SAMPLE_VI_FRAME_INFO_S stViFrameInfo;
    ISP_FPN_CALIBRATE_ATTR_S stCaliAttr;
    HI_S32 s32Ret, i;
    VI_CHN ViChn = 0;

    g_fpn_status = FPN_STATUS_FAILED;

    /* 获取 ISP 属性 (分辨率) */
    if (HI_SUCCESS != HI_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr)) {
        printf("[FPN] GetPubAttr failed\n");
        return g_fpn_status;
    }

    /* 配置帧缓冲 */
    stFrmCfg.u32Width      = stPubAttr.stWndRect.u32Width;
    stFrmCfg.u32Height     = stPubAttr.stWndRect.u32Height;
    stFrmCfg.u32ByteAlign  = 0;
    stFrmCfg.enPixelFormat = PIXEL_FORMAT_RGB_BAYER_16BPP;
    stFrmCfg.enCompressMode = COMPRESS_MODE_NONE;
    stFrmCfg.enVideoFormat = VIDEO_FORMAT_LINEAR;
    stFrmCfg.enDynamicRange = DYNAMIC_RANGE_SDR8;

    if (HI_SUCCESS != SAMPLE_VI_GetFrameBlkInfo(&stFrmCfg, 1, &stViFrameInfo)) {
        printf("[FPN] GetFrameBlkInfo failed\n");
        return g_fpn_status;
    }

    /* 停 VI 通道, 执行 FPN 校准 (捕获暗帧) */
    HI_MPI_VI_DisableChn(ViPipe, ViChn);

    memset(&stCaliAttr, 0, sizeof(stCaliAttr));
    stCaliAttr.enFpnType   = ISP_FPN_TYPE_FRAME;
    stCaliAttr.u32FrameNum = 16;
    stCaliAttr.u32Threshold = 4095;
    for (i = 0; i < VI_MAX_SPLIT_NODE_NUM; i++)
        stCaliAttr.stFpnCaliFrame.u32Offset[i] = 0;
    stCaliAttr.stFpnCaliFrame.u32Iso     = 0;
    stCaliAttr.stFpnCaliFrame.u32FrmSize = stViFrameInfo.u32Size;
    memcpy(&stCaliAttr.stFpnCaliFrame.stFpnFrame,
           &stViFrameInfo.stVideoFrameInfo, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = HI_MPI_ISP_FPNCalibrate(ViPipe, &stCaliAttr);
    printf("[FPN] FPNCalibrate ret=0x%x, ISO=%u\n",
           s32Ret, stCaliAttr.stFpnCaliFrame.u32Iso);

    HI_MPI_VI_EnableChn(ViPipe, ViChn);
    SAMPLE_VI_COMM_ReleaseFrameBlkInfo(1, &stViFrameInfo);

    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] calibration failed\n");
        return g_fpn_status;
    }

    /* 暗帧质量检测 */
    if (!check_dark_frame(&caliInfo, &stCaliAttr)) {
        g_fpn_status = FPN_STATUS_NEED_RETRY;
        printf("[FPN] dark frame not qualified, need manual retry\n");
        return g_fpn_status;
    }

    /* 启用 FPN 校正 */
    memset(&corrInfo, 0, sizeof(corrInfo));
    corrInfo.enOpType       = OP_TYPE_AUTO;
    corrInfo.enFpnType      = ISP_FPN_TYPE_FRAME;
    corrInfo.u32Strength    = 0;
    corrInfo.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    corrInfo.enCompressMode = COMPRESS_MODE_NONE;

    s32Ret = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] enable correction failed: 0x%x\n", s32Ret);
        return g_fpn_status;
    }

    g_fpn_status = FPN_STATUS_OK;
    printf("[FPN] calibration OK, correction enabled\n");
    return g_fpn_status;
}

fpn_status_t mpp_fpn_retry(VI_PIPE ViPipe)
{
    printf("[FPN] manual retry...\n");
    return mpp_fpn_calibrate(ViPipe);
}
