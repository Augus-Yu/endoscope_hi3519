/**
 * @file mpp_fpn.c
 * @brief FPN (Fixed Pattern Noise) 自动校准和校正
 * @details 参照 SDK sample_vio.c SAMPLE_VIO_ViFPN() 实现
 */

#include "mpp_fpn.h"
#include "mpp_video.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define OV6946_I2C_BUS  2
#define OV6946_I2C_ADDR 0x36

/* SDK sample_comm_vi.c 内部函数 (未在 sample_comm.h 公开) */
extern HI_S32 SAMPLE_VI_GetFrameBlkInfo(SAMPLE_VI_FRAME_CONFIG_S *pstFrmCfg,
    HI_S32 s32FrmCnt, SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);
extern HI_VOID SAMPLE_VI_COMM_ReleaseFrameBlkInfo(HI_S32 s32FrmCnt,
    SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);
extern HI_VOID SAMPLE_COMM_VI_SaveFpnFile(
    ISP_FPN_TYPE_E, ISP_FPN_FRAME_INFO_S *, FILE *);

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
    ov6946_write(0x3501, 0x00);
    ov6946_write(0x3502, 0x00);
    ov6946_write(0x350B, 0x00);
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

static void fpn_led_off(void)
{
    int fd = open("/dev/lm3630a", O_RDWR);
    if (fd < 0) return;
    g_led_saved = 3;
    char val = 0;
    write(fd, &val, 1);
    close(fd);
    printf("[FPN] LED off\n");
}

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

/* ── 辅助: 生成 FPN 文件名 (参照 SDK PRINT_FPNTYPE) ── */

static const char *fpn_type_name(ISP_FPN_TYPE_E t)
{
    return (t == ISP_FPN_TYPE_FRAME) ? "Frame" : "Line";
}

static void fpn_make_filename(char *buf, size_t len, ISP_FPN_TYPE_E type,
                               HI_U32 w, HI_U32 h, HI_U32 bit)
{
    snprintf(buf, len, "./FPN_%s_w%d_h%d_%dbit.raw",
             fpn_type_name(type), w, h, bit);
}

/* ── FPN 状态 ── */

fpn_status_t mpp_fpn_get_status(void)
{
    return g_fpn_status;
}

/* ── 校准: 采集暗帧 → FPNCalibrate → 保存文件 ── */

fpn_status_t mpp_fpn_calibrate(VI_PIPE ViPipe)
{
    SAMPLE_VI_FPN_CALIBRATE_INFO_S caliInfo;
    SAMPLE_VI_FPN_CORRECTION_INFO_S corrInfo;
    ISP_PUB_ATTR_S stPubAttr;
    ISP_FPN_CALIBRATE_ATTR_S stCaliAttr;
    SAMPLE_VI_FRAME_CONFIG_S stFrmCfg;
    SAMPLE_VI_FRAME_INFO_S stViFrameInfo;
    HI_S32 s32Ret, i;
    VI_CHN ViChn = 0;

    g_fpn_status = FPN_STATUS_FAILED;

    video_context_t *vctx = video_get_context();
    if (!vctx) { printf("[FPN] no video context\n"); return g_fpn_status; }

    /* ── 准备暗环境 ── */
    fpn_led_off();
    ov6946_kill_exposure();

    ISP_EXPOSURE_ATTR_S savedExp;
    ISP_BLACK_LEVEL_S savedBL;
    HI_MPI_ISP_GetExposureAttr(ViPipe, &savedExp);
    HI_MPI_ISP_GetBlackLevelAttr(ViPipe, &savedBL);

    /* ── 切换 VI → ONLINE (FPN 校准要求) ── */
    printf("[FPN] switching VI to online mode...\n");
    s32Ret = SAMPLE_COMM_VI_StopVi(&vctx->vi_config);
    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] StopVi failed: 0x%x\n", s32Ret);
        goto restore_hw;
    }
    vctx->vi_config.astViInfo[vctx->sensor->sns_id]
        .stPipeInfo.enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;
    s32Ret = SAMPLE_COMM_VI_StartVi(&vctx->vi_config);
    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] VI online start failed: 0x%x\n", s32Ret);
        goto restore_hw;
    }
    printf("[FPN] VI in ONLINE mode\n");
    usleep(50000);

    /* StartVi 重置传感器, 需再次杀曝光 */
    ov6946_write(0x3501, 0x00);
    ov6946_write(0x3502, 0x00);
    ov6946_write(0x350B, 0x00);
    printf("[FPN] exposure re-killed after VI start\n");

    /* ISP 曝光最小 + 黑电平最大 */
    {
        ISP_EXPOSURE_ATTR_S exp = savedExp;
        exp.bByPass  = HI_TRUE;
        exp.enOpType = OP_TYPE_MANUAL;
        exp.stManual.enExpTimeOpType = OP_TYPE_MANUAL;
        exp.stManual.enAGainOpType   = OP_TYPE_MANUAL;
        exp.stManual.enDGainOpType   = OP_TYPE_MANUAL;
        exp.stManual.enISPDGainOpType = OP_TYPE_MANUAL;
        exp.stManual.u32ExpTime  = 1;
        exp.stManual.u32AGain    = 0x400;
        exp.stManual.u32DGain    = 0x400;
        exp.stManual.u32ISPDGain = 0x400;
        HI_MPI_ISP_SetExposureAttr(ViPipe, &exp);
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
        printf("[FPN] ISP: min exposure, max black level\n");
    }
    usleep(100000);

    /* ── 配置校准参数 (参照 SDK SAMPLE_COMM_VI_FpnCalibrateConfig) ── */
    if (HI_SUCCESS != HI_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr)) {
        printf("[FPN] GetPubAttr failed\n");
        goto restore_vi;
    }

    stFrmCfg.u32Width       = stPubAttr.stWndRect.u32Width;
    stFrmCfg.u32Height      = stPubAttr.stWndRect.u32Height;
    stFrmCfg.u32ByteAlign   = 0;
    stFrmCfg.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    stFrmCfg.enCompressMode = COMPRESS_MODE_NONE;
    stFrmCfg.enVideoFormat  = VIDEO_FORMAT_LINEAR;
    stFrmCfg.enDynamicRange = DYNAMIC_RANGE_SDR8;

    if (HI_SUCCESS != SAMPLE_VI_GetFrameBlkInfo(&stFrmCfg, 1, &stViFrameInfo)) {
        printf("[FPN] GetFrameBlkInfo failed\n");
        goto restore_vi;
    }
    printf("[FPN] Frame buffer: %dx%d\n", stFrmCfg.u32Width, stFrmCfg.u32Height);

    HI_MPI_VI_DisableChn(ViPipe, ViChn);

    /* ── 填充校准参数 (与 SDK 一致) ── */
    caliInfo.u32Threshold   = 4095;
    caliInfo.u32FrameNum    = 16;
    caliInfo.enFpnType      = ISP_FPN_TYPE_FRAME;
    caliInfo.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    caliInfo.enCompressMode = COMPRESS_MODE_NONE;

    memset(&stCaliAttr, 0, sizeof(stCaliAttr));
    stCaliAttr.enFpnType    = caliInfo.enFpnType;
    stCaliAttr.u32FrameNum  = caliInfo.u32FrameNum;
    stCaliAttr.u32Threshold = caliInfo.u32Threshold;
    stCaliAttr.stFpnCaliFrame.u32Iso = 0;
    for (i = 0; i < VI_MAX_SPLIT_NODE_NUM; i++)
        stCaliAttr.stFpnCaliFrame.u32Offset[i] = 0;
    stCaliAttr.stFpnCaliFrame.u32FrmSize = stViFrameInfo.u32Size;
    memcpy(&stCaliAttr.stFpnCaliFrame.stFpnFrame,
           &stViFrameInfo.stVideoFrameInfo, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = HI_MPI_ISP_FPNCalibrate(ViPipe, &stCaliAttr);
    printf("[FPN] FPNCalibrate ret=0x%x, ISO=%u\n",
           s32Ret, stCaliAttr.stFpnCaliFrame.u32Iso);

    if (HI_SUCCESS == s32Ret) {
        /* 保存 FPN 文件 (参照 SDK SAMPLE_COMM_VI_SaveFpnFile) */
        char fn[128];
        fpn_make_filename(fn, sizeof(fn), caliInfo.enFpnType,
                          stPubAttr.stWndRect.u32Width,
                          stPubAttr.stWndRect.u32Height, 16);
        printf("[FPN] saving to %s...\n", fn);
        FILE *fp = fopen(fn, "wb");
        if (fp) {
            SAMPLE_COMM_VI_SaveFpnFile(stCaliAttr.enFpnType,
                                       &stCaliAttr.stFpnCaliFrame, fp);
            fclose(fp);
            printf("[FPN] saved %s\n", fn);
        } else {
            printf("[FPN] FAILED to open %s\n", fn);
        }
    }

    HI_MPI_VI_EnableChn(ViPipe, ViChn);
    SAMPLE_VI_COMM_ReleaseFrameBlkInfo(1, &stViFrameInfo);

    if (HI_SUCCESS != s32Ret) {
        printf("[FPN] calibration failed\n");
        goto restore_vi;
    }

    g_fpn_status = FPN_STATUS_OK;
    printf("[FPN] calibration OK\n");

restore_vi:
    /* ── 切回 OFFLINE ── */
    ov6946_restore_exposure();
    printf("[FPN] restoring VI to offline mode...\n");
    SAMPLE_COMM_VI_StopVi(&vctx->vi_config);
    vctx->vi_config.astViInfo[vctx->sensor->sns_id]
        .stPipeInfo.enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
    SAMPLE_COMM_VI_StartVi(&vctx->vi_config);
    HI_MPI_ISP_SetExposureAttr(ViPipe, &savedExp);
    HI_MPI_ISP_SetBlackLevelAttr(ViPipe, &savedBL);
    fpn_led_restore();
    printf("[FPN] VI offline, exposure/BL/LED restored\n");

    /* ── 启用 FPN 校正 (参照 SDK: StartVi 后立即 SAMPLE_COMM_VI_FpnCorrectionConfig) ── */
    if (g_fpn_status == FPN_STATUS_OK) {
        memset(&corrInfo, 0, sizeof(corrInfo));
        corrInfo.enOpType       = OP_TYPE_AUTO;
        corrInfo.enFpnType      = caliInfo.enFpnType;
        corrInfo.u32Strength    = 0;
        corrInfo.enPixelFormat  = caliInfo.enPixelFormat;
        corrInfo.enCompressMode = caliInfo.enCompressMode;

        HI_MPI_VI_DisableChn(ViPipe, ViChn);
        s32Ret = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
        HI_MPI_VI_EnableChn(ViPipe, ViChn);

        if (HI_SUCCESS != s32Ret) {
            printf("[FPN] enable correction failed: 0x%x\n", s32Ret);
            g_fpn_status = FPN_STATUS_FAILED;
        } else {
            printf("[FPN] correction enabled\n");
        }
    }

    return g_fpn_status;

restore_hw:
    HI_MPI_ISP_SetExposureAttr(ViPipe, &savedExp);
    fpn_led_restore();
    return g_fpn_status;
}

/* ── 加载已有 FPN 文件并启用校正 ── */

fpn_status_t mpp_fpn_load(VI_PIPE ViPipe)
{
    ISP_PUB_ATTR_S stPubAttr;
    char fn[128];

    g_fpn_status = FPN_STATUS_FAILED;

    if (HI_SUCCESS != HI_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr))
        return g_fpn_status;

    fpn_make_filename(fn, sizeof(fn), ISP_FPN_TYPE_FRAME,
                      stPubAttr.stWndRect.u32Width,
                      stPubAttr.stWndRect.u32Height, 16);

    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        printf("[FPN] no calibration file, running auto-calibrate...\n");
        g_fpn_status = mpp_fpn_calibrate(ViPipe);
        return g_fpn_status;
    }
    fclose(fp);

    /* 参照 SDK SAMPLE_COMM_VI_FpnCorrectionConfig 启用校正 */
    {
        SAMPLE_VI_FPN_CORRECTION_INFO_S corrInfo;
        memset(&corrInfo, 0, sizeof(corrInfo));
        corrInfo.enOpType       = OP_TYPE_AUTO;
        corrInfo.enFpnType      = ISP_FPN_TYPE_FRAME;
        corrInfo.u32Strength    = 0;
        corrInfo.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
        corrInfo.enCompressMode = COMPRESS_MODE_NONE;

        HI_MPI_VI_DisableChn(ViPipe, 0);
        HI_S32 r = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
        HI_MPI_VI_EnableChn(ViPipe, 0);

        if (HI_SUCCESS != r) {
            printf("[FPN] load/enable correction failed: 0x%x\n", r);
            return g_fpn_status;
        }
    }

    g_fpn_status = FPN_STATUS_OK;
    printf("[FPN] loaded calibration, correction enabled\n");
    return g_fpn_status;
}

/* ── 仅加载文件 (管线复用时用, 不自动校准) ── */

fpn_status_t mpp_fpn_load_file_only(VI_PIPE ViPipe)
{
    ISP_PUB_ATTR_S stPubAttr;
    char fn[128];

    g_fpn_status = FPN_STATUS_FAILED;

    if (HI_SUCCESS != HI_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr))
        return g_fpn_status;

    fpn_make_filename(fn, sizeof(fn), ISP_FPN_TYPE_FRAME,
                      stPubAttr.stWndRect.u32Width,
                      stPubAttr.stWndRect.u32Height, 16);

    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        printf("[FPN] no calibration file, skip (pipeline reused)\n");
        return g_fpn_status;
    }
    fclose(fp);

    SAMPLE_VI_FPN_CORRECTION_INFO_S corrInfo;
    memset(&corrInfo, 0, sizeof(corrInfo));
    corrInfo.enOpType       = OP_TYPE_AUTO;
    corrInfo.enFpnType      = ISP_FPN_TYPE_FRAME;
    corrInfo.u32Strength    = 0;
    corrInfo.enPixelFormat  = PIXEL_FORMAT_RGB_BAYER_16BPP;
    corrInfo.enCompressMode = COMPRESS_MODE_NONE;

    HI_MPI_VI_DisableChn(ViPipe, 0);
    HI_S32 r = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
    HI_MPI_VI_EnableChn(ViPipe, 0);

    if (HI_SUCCESS != r) {
        printf("[FPN] load/enable correction failed: 0x%x\n", r);
        return g_fpn_status;
    }

    g_fpn_status = FPN_STATUS_OK;
    printf("[FPN] loaded calibration (reused session), correction enabled\n");
    return g_fpn_status;
}

/* ── 手动重试 ── */

fpn_status_t mpp_fpn_retry(VI_PIPE ViPipe)
{
    printf("[FPN] manual retry...\n");
    return mpp_fpn_calibrate(ViPipe);
}
