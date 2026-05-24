/**
 * @file mpp_fpn.c
 * @brief FPN (Fixed Pattern Noise) 自动校准和校正
 */

#include "mpp_fpn.h"
#include "mpp_video.h"
#include "mpi_ae.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define FPN_TEST_DARK_FRAME 0
#define OV6946_I2C_BUS  2
#define OV6946_I2C_ADDR 0x36

extern HI_S32 SAMPLE_VI_GetFrameBlkInfo(SAMPLE_VI_FRAME_CONFIG_S *pstFrmCfg,
    HI_S32 s32FrmCnt, SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);
extern HI_VOID SAMPLE_VI_COMM_ReleaseFrameBlkInfo(HI_S32 s32FrmCnt,
    SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);

static fpn_status_t g_fpn_status = FPN_STATUS_IDLE;
static int g_led_saved = -1;

/* ── OV6946 I2C 寄存器操作 ── */

static int ov6946_write(HI_U16 reg, HI_U8 val)
{
    char fn[20]; int fd, ret;
    unsigned char buf[3];
    snprintf(fn, sizeof(fn), "/dev/i2c-%d", OV6946_I2C_BUS);
    fd = open(fn, O_RDWR);
    if (fd < 0) return -1;
    ret = ioctl(fd, I2C_SLAVE_FORCE, OV6946_I2C_ADDR);
    if (ret < 0) { close(fd); return -1; }
    buf[0] = (reg >> 8) & 0xFF;
    buf[1] = reg & 0xFF;
    buf[2] = val & 0xFF;
    ret = write(fd, buf, 3);
    close(fd);
    return (ret == 3) ? 0 : -1;
}

static int ov6946_read(HI_U16 reg, HI_U8 *val)
{
    char fn[20]; int fd, ret;
    unsigned char wbuf[2];
    snprintf(fn, sizeof(fn), "/dev/i2c-%d", OV6946_I2C_BUS);
    fd = open(fn, O_RDWR);
    if (fd < 0) return -1;
    ret = ioctl(fd, I2C_SLAVE_FORCE, OV6946_I2C_ADDR);
    if (ret < 0) { close(fd); return -1; }
    wbuf[0] = (reg >> 8) & 0xFF;
    wbuf[1] = reg & 0xFF;
    struct i2c_msg msgs[2] = {
        { .addr = OV6946_I2C_ADDR, .flags = 0,        .len = 2, .buf = wbuf },
        { .addr = OV6946_I2C_ADDR, .flags = I2C_M_RD, .len = 1, .buf = val  },
    };
    struct i2c_rdwr_ioctl_data rdwr = { .msgs = msgs, .nmsgs = 2 };
    ret = ioctl(fd, I2C_RDWR, &rdwr);
    close(fd);
    return (ret == 2) ? 0 : -1;
}

static HI_U8 saved_exp_3501, saved_exp_3502, saved_gain_350b;
static int sensor_regs_saved;

static void ov6946_kill_exposure(void)
{
    sensor_regs_saved = 0;
    if (ov6946_read(0x3501, &saved_exp_3501) < 0 ||
        ov6946_read(0x3502, &saved_exp_3502) < 0 ||
        ov6946_read(0x350B, &saved_gain_350b) < 0) {
        printf("[FPN] OV6946 I2C read failed\n");
        return;
    }
    sensor_regs_saved = 1;
    ov6946_write(0x3501, 0x00); /* 曝光=0 */
    ov6946_write(0x3502, 0x00);
    ov6946_write(0x350B, 0x00); /* 增益=0 */
    printf("[FPN] OV6946 exposure killed (was 3501=0x%02x 3502=0x%02x 350B=0x%02x)\n",
           saved_exp_3501, saved_exp_3502, saved_gain_350b);
}

static void ov6946_restore_exposure(void)
{
    if (!sensor_regs_saved) return;
    ov6946_write(0x3501, saved_exp_3501);
    ov6946_write(0x3502, saved_exp_3502);
    ov6946_write(0x350B, saved_gain_350b);
    sensor_regs_saved = 0;
    printf("[FPN] OV6946 exposure restored\n");
}

/* 关 LED (保存当前值, 写入0) */
static void fpn_led_off(void)
{
    int fd = open("/dev/lm3630a", O_RDWR);
    if (fd < 0) return;
    /* 读取当前亮度 (不支持读, 只能写, 用变量记录) */
    g_led_saved = 3; /* 默认假设3档, 后续调用恢复 */
    char val = 0;
    write(fd, &val, 1);
    close(fd);
    printf("[FPN] LED off\n");
}

/* 恢复 LED */
static void fpn_led_restore(void)
{
    if (g_led_saved < 0) return;
    int fd = open("/dev/lm3630a", O_RDWR);
    if (fd < 0) return;
    char val = (char)g_led_saved;
    write(fd, &val, 1);
    close(fd);
    g_led_saved = -1;
    printf("[FPN] LED restored to %d\n", val);
}

fpn_status_t mpp_fpn_get_status(void)
{
    return g_fpn_status;
}

fpn_status_t mpp_fpn_calibrate(VI_PIPE ViPipe)
{
    SAMPLE_VI_FPN_CORRECTION_INFO_S corrInfo;
    ISP_PUB_ATTR_S stPubAttr;
    SAMPLE_VI_FRAME_CONFIG_S stFrmCfg;
    SAMPLE_VI_FRAME_INFO_S stViFrameInfo;
    ISP_FPN_CALIBRATE_ATTR_S stCaliAttr;
    HI_S32 s32Ret, i;
    VI_CHN ViChn = 0;

    g_fpn_status = FPN_STATUS_FAILED;

    video_context_t *vctx = video_get_context();
    if (!vctx) { printf("[FPN] no video context\n"); return g_fpn_status; }

    /* 关 LED + 传感器曝光清零 (先保存原始值; StartVi 会重置传感器，后面再补杀) */
    fpn_led_off();
    ov6946_kill_exposure();

    /* 保存当前 ISP 曝光和黑电平 (用于后续恢复) */
    ISP_EXPOSURE_ATTR_S savedExp;
    ISP_BLACK_LEVEL_S savedBL;
    HI_MPI_ISP_GetExposureAttr(ViPipe, &savedExp);
    HI_MPI_ISP_GetBlackLevelAttr(ViPipe, &savedBL);

    /* FPN 硬件要求 ONLINE 模式 (冷启动时 VI 新鲜初始化的, 可以切) */
    printf("[FPN] switching VI to online mode...\n");
    s32Ret = SAMPLE_COMM_VI_StopVi(&vctx->vi_config);
    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] StopVi failed: 0x%x\n", s32Ret);
#if !FPN_TEST_DARK_FRAME
        HI_MPI_ISP_SetExposureAttr(ViPipe, &savedExp);
        fpn_led_restore();
#endif
        return g_fpn_status;
    }
    vctx->vi_config.astViInfo[vctx->sensor->sns_id]
        .stPipeInfo.enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;
    s32Ret = SAMPLE_COMM_VI_StartVi(&vctx->vi_config);
    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] VI online start failed: 0x%x\n", s32Ret);
#if !FPN_TEST_DARK_FRAME
        HI_MPI_ISP_SetExposureAttr(ViPipe, &savedExp);
        fpn_led_restore();
#endif
        return g_fpn_status;
    }
    printf("[FPN] VI in ONLINE mode\n");
    usleep(50000);

    /* StartVi 会重置传感器寄存器, 需再次杀曝光 (不经过 kill_exposure, 避免覆盖原始保存值) */
    ov6946_write(0x3501, 0x00);
    ov6946_write(0x3502, 0x00);
    ov6946_write(0x350B, 0x00);
    printf("[FPN] OV6946 exposure re-killed after VI start\n");

    /* ISP 曝光最小 + 黑电平拉满 (必须在 ONLINE 模式之后设置，否则 StartVi 会重置 ISP) */
    ISP_EXPOSURE_ATTR_S minExp = savedExp;
    minExp.bByPass  = HI_TRUE;
    minExp.enOpType = OP_TYPE_MANUAL;
    minExp.stManual.enExpTimeOpType = OP_TYPE_MANUAL;
    minExp.stManual.enAGainOpType   = OP_TYPE_MANUAL;
    minExp.stManual.enDGainOpType   = OP_TYPE_MANUAL;
    minExp.stManual.enISPDGainOpType = OP_TYPE_MANUAL;
    minExp.stManual.u32ExpTime  = 1;
    minExp.stManual.u32AGain    = 0x400;
    minExp.stManual.u32DGain    = 0x400;
    minExp.stManual.u32ISPDGain = 0x400;
    s32Ret = HI_MPI_ISP_SetExposureAttr(ViPipe, &minExp);
    printf("[FPN] SetExposureAttr ret=0x%x, exp=%d gain=%d/%d/%d\n",
           s32Ret, minExp.stManual.u32ExpTime,
           minExp.stManual.u32AGain, minExp.stManual.u32DGain,
           minExp.stManual.u32ISPDGain);
    {
        ISP_EXPOSURE_ATTR_S verify;
        HI_MPI_ISP_GetExposureAttr(ViPipe, &verify);
        printf("[FPN] Verify: op=%d exp=%d again=%d\n",
               verify.enOpType, verify.stManual.u32ExpTime,
               verify.stManual.u32AGain);
    }

    {
        ISP_BLACK_LEVEL_S bl;
        HI_MPI_ISP_GetBlackLevelAttr(ViPipe, &bl);
        bl.enOpType = OP_TYPE_MANUAL;
        bl.au16BlackLevel[0] = 4095;
        bl.au16BlackLevel[1] = 4095;
        bl.au16BlackLevel[2] = 4095;
        bl.au16BlackLevel[3] = 4095;
        HI_MPI_ISP_SetBlackLevelAttr(ViPipe, &bl);
        printf("[FPN] BlackLevel set to max\n");
    }
    usleep(100000);

    /* 获取 ISP 属性 (分辨率) */
    if (HI_SUCCESS != HI_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr)) {
        printf("[FPN] GetPubAttr failed\n");
        goto restore;
    }

    /* 配置帧缓冲 */
    stFrmCfg.u32Width       = stPubAttr.stWndRect.u32Width;
    stFrmCfg.u32Height      = stPubAttr.stWndRect.u32Height;
    stFrmCfg.u32ByteAlign   = 0;
    stFrmCfg.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    stFrmCfg.enCompressMode = COMPRESS_MODE_NONE;
    stFrmCfg.enVideoFormat  = VIDEO_FORMAT_LINEAR;
    stFrmCfg.enDynamicRange = DYNAMIC_RANGE_SDR8;

    if (HI_SUCCESS != SAMPLE_VI_GetFrameBlkInfo(&stFrmCfg, 1, &stViFrameInfo)) {
        printf("[FPN] GetFrameBlkInfo failed\n");
        goto restore;
    }

    /* 停 VI 通道, 执行 FPN 校准 */
    HI_MPI_VI_DisableChn(ViPipe, ViChn);

    memset(&stCaliAttr, 0, sizeof(stCaliAttr));
    stCaliAttr.enFpnType    = ISP_FPN_TYPE_FRAME;
    stCaliAttr.u32FrameNum  = 16;
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

    if (HI_SUCCESS == s32Ret) {
        /* 保存 FPN 校准数据到文件 */
        extern HI_VOID SAMPLE_COMM_VI_SaveFpnFile(
            ISP_FPN_TYPE_E, ISP_FPN_FRAME_INFO_S *, FILE *);
        char fn[128];
        snprintf(fn, sizeof(fn), "./FPN_Frame_w%d_h%d_%dbit.raw",
                 stPubAttr.stWndRect.u32Width,
                 stPubAttr.stWndRect.u32Height,
                 16);
        printf("[FPN] saving to %s...\n", fn);
        FILE *fp = fopen(fn, "wb");
        if (fp) {
            SAMPLE_COMM_VI_SaveFpnFile(stCaliAttr.enFpnType,
                &stCaliAttr.stFpnCaliFrame, fp);
            fclose(fp);
            printf("[FPN] saved calibration to %s\n", fn);
        } else {
            printf("[FPN] FAILED to open %s for writing\n", fn);
        }
    }

    HI_MPI_VI_EnableChn(ViPipe, ViChn);
    SAMPLE_VI_COMM_ReleaseFrameBlkInfo(1, &stViFrameInfo);

    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] calibration failed\n");
        goto restore;
    }

    /* 启用 FPN 校正 (用户已确认遮光, 跳过ISO检测) */
    memset(&corrInfo, 0, sizeof(corrInfo));
    corrInfo.enOpType       = OP_TYPE_AUTO;
    corrInfo.enFpnType      = ISP_FPN_TYPE_FRAME;
    corrInfo.u32Strength    = 0;
    corrInfo.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    corrInfo.enCompressMode = COMPRESS_MODE_NONE;

    s32Ret = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] enable correction failed: 0x%x\n", s32Ret);
        goto restore;
    }

    g_fpn_status = FPN_STATUS_OK;
    printf("[FPN] calibration OK, correction enabled\n");

restore:
    /* 恢复传感器曝光 (用模式切换前保存的原始值) */
    ov6946_restore_exposure();
#if FPN_TEST_DARK_FRAME
    printf("[FPN] TEST: restoring VI offline (keeping min exp)...\n");
    SAMPLE_COMM_VI_StopVi(&vctx->vi_config);
    vctx->vi_config.astViInfo[vctx->sensor->sns_id]
        .stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    SAMPLE_COMM_VI_StartVi(&vctx->vi_config);
#else
    printf("[FPN] restoring VI to offline mode...\n");
    SAMPLE_COMM_VI_StopVi(&vctx->vi_config);
    vctx->vi_config.astViInfo[vctx->sensor->sns_id]
        .stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    SAMPLE_COMM_VI_StartVi(&vctx->vi_config);
    HI_MPI_ISP_SetExposureAttr(ViPipe, &savedExp);
    HI_MPI_ISP_SetBlackLevelAttr(ViPipe, &savedBL);
    fpn_led_restore();
    printf("[FPN] VI offline, exposure/BL/LED restored\n");
#endif
    return g_fpn_status;
}

fpn_status_t mpp_fpn_load(VI_PIPE ViPipe)
{
    SAMPLE_VI_FPN_CORRECTION_INFO_S corrInfo;
    ISP_PUB_ATTR_S stPubAttr;
    FILE *fp;

    g_fpn_status = FPN_STATUS_FAILED;

    /* 检查 FPN 数据文件是否存在 */
    if (HI_SUCCESS != HI_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr))
        return g_fpn_status;

    char fn[128];
    snprintf(fn, sizeof(fn), "./FPN_Frame_w%d_h%d_16bit.raw",
             stPubAttr.stWndRect.u32Width, stPubAttr.stWndRect.u32Height);

    fp = fopen(fn, "rb");
    if (!fp) {
        printf("[FPN] no calibration file, running auto-calibrate...\n");
        g_fpn_status = mpp_fpn_calibrate(ViPipe);
        return g_fpn_status;
    }
    fclose(fp);

    /* 加载并启用校正 (需要先停VI通道) */
    HI_MPI_VI_DisableChn(ViPipe, 0);

    memset(&corrInfo, 0, sizeof(corrInfo));
    corrInfo.enOpType       = OP_TYPE_AUTO;
    corrInfo.enFpnType      = ISP_FPN_TYPE_FRAME;
    corrInfo.u32Strength    = 0;
    corrInfo.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    corrInfo.enCompressMode = COMPRESS_MODE_NONE;

    HI_S32 r = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
    HI_MPI_VI_EnableChn(ViPipe, 0);

    if (HI_SUCCESS != r) {
        printf("[FPN] load/enable correction failed: 0x%x\n", r);
        return g_fpn_status;
    }

    g_fpn_status = FPN_STATUS_OK;
    printf("[FPN] loaded calibration, correction enabled\n");
    return g_fpn_status;
}

void mpp_fpn_test_dark_frame(VI_PIPE ViPipe)
{
    /* LED 关 */
    int fd = open("/dev/lm3630a", O_RDWR);
    if (fd >= 0) { char v = 0; write(fd, &v, 1); close(fd); }
    /* 传感器曝光清零 */
    ov6946_kill_exposure();
    /* ISP 最小曝光 */
    ISP_EXPOSURE_ATTR_S exp;
    if (HI_SUCCESS == HI_MPI_ISP_GetExposureAttr(ViPipe, &exp)) {
        exp.bByPass = HI_TRUE; exp.enOpType = OP_TYPE_MANUAL;
        exp.stManual.enExpTimeOpType = OP_TYPE_MANUAL;
        exp.stManual.enAGainOpType   = OP_TYPE_MANUAL;
        exp.stManual.enDGainOpType   = OP_TYPE_MANUAL;
        exp.stManual.enISPDGainOpType = OP_TYPE_MANUAL;
        exp.stManual.u32ExpTime = 1;
        exp.stManual.u32AGain   = 0x400;
        exp.stManual.u32DGain   = 0x400;
        exp.stManual.u32ISPDGain = 0x400;
        HI_MPI_ISP_SetExposureAttr(ViPipe, &exp);
        printf("[FPN-TEST] ISP exposure set to min\n");
    }
    /* 黑电平也拉满 */
    ISP_BLACK_LEVEL_S bl;
    if (HI_SUCCESS == HI_MPI_ISP_GetBlackLevelAttr(ViPipe, &bl)) {
        bl.enOpType = OP_TYPE_MANUAL;
        bl.au16BlackLevel[0] = 4095; bl.au16BlackLevel[1] = 4095;
        bl.au16BlackLevel[2] = 4095; bl.au16BlackLevel[3] = 4095;
        HI_MPI_ISP_SetBlackLevelAttr(ViPipe, &bl);
        printf("[FPN-TEST] BlackLevel set to max\n");
    }
    printf("[FPN-TEST] dark frame active\n");
}

fpn_status_t mpp_fpn_retry(VI_PIPE ViPipe)
{
    printf("[FPN] manual retry...\n");
    return mpp_fpn_calibrate(ViPipe);
}
