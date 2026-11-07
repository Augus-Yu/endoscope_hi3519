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
#include <sys/stat.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define OV6946_I2C_BUS  2
#define OV6946_I2C_ADDR 0x36

/* FPN 校正强度 —— 仅在 OP_TYPE_MANUAL 下有效 (HiISP 开发参考 6.15.3):
 * 范围 [0,1023], 1 倍强度 = 256。自动模式下该值被忽略。 */
#define FPN_CORRECTION_STRENGTH 256

/* 用 OP_TYPE_AUTO 而非 MANUAL:
 * HiISP 开发参考 6.15.3 明确 ISP_FPN_AUTO_ATTR_S.u32Strength 是"只读属性,
 * ISP FWM 根据 ISO 自动调整", 所以 SDK 不写 stAuto 并不是问题 —— 之前担心的
 * "未初始化栈残留"并不存在。自动模式按"当前 ISO ÷ 标定 ISO"缩放强度,
 * 正好匹配乘性(增益相关)的竖条纹。 */

/* SDK sample_comm_vi.c 内部函数 (未在 sample_comm.h 公开) */
extern HI_S32 SAMPLE_VI_GetFrameBlkInfo(SAMPLE_VI_FRAME_CONFIG_S *pstFrmCfg,
    HI_S32 s32FrmCnt, SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);
extern HI_VOID SAMPLE_VI_COMM_ReleaseFrameBlkInfo(HI_S32 s32FrmCnt,
    SAMPLE_VI_FRAME_INFO_S *pastViFrameInfo);
extern HI_VOID SAMPLE_COMM_VI_SaveFpnFile(
    ISP_FPN_TYPE_E, ISP_FPN_FRAME_INFO_S *, FILE *);

static fpn_status_t g_fpn_status = FPN_STATUS_IDLE;
static int g_led_saved = -1;

/* 调试开关: 置 1 时跳过所有"变暗"步骤, 采样一帧亮场数据。
 * 用途: 判断竖条纹到底存不存在于 VI 原始数据里 —— 保持其他环节不变,
 * 只把"关灯/杀曝光"换成正常曝光, 采出来的表就能和暗场表逐列对比。 */
static int g_fpn_skip_darken = 0;
#define FPN_LIT_DUMP_FILE "./FPN_Lit_w%d_h%d_%dbit.raw"

/* ── OV6946 I2C 寄存器操作 ──
 * 只保留读: 校准不再改写传感器寄存器 (旧实现写 0 会破坏"在工作增益下采黑帧"
 * 这一前提)。如果以后需要写寄存器, 从 git 历史里恢复 ov6946_write 即可。 */

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

/* 文档 6.15.1 要求"在 FPN 最严重的 ISO 下固定增益"再遮光采黑帧。
 * 所以这里只读传感器寄存器做诊断, 绝不改写 —— 增益由 ISP AE 驱动,
 * 下面用 QueryExposureInfo 拿到的当前值把 AE 冻结住即可。
 *
 * 旧实现把 0x3501/0x3502/0x350B 全写 0, 等于在 1x 增益下采黑帧; 而竖条纹
 * 是乘性的, 在 1x 增益下几乎消失, 采出来的校正表是平的 —— 这才是竖条纹
 * 一直去不掉的根因。 */
static void ov6946_log_exposure(void)
{
    HI_U8 e1 = 0, e2 = 0, g = 0;

    if (ov6946_read(0x3501, &e1) == 0 &&
        ov6946_read(0x3502, &e2) == 0 &&
        ov6946_read(0x350B, &g) == 0) {
        printf("[FPN] OV6946 regs: 3501=0x%02x 3502=0x%02x (exposure) 350B=0x%02x (gain)\n",
               e1, e2, g);
    } else {
        printf("[FPN] OV6946 I2C read failed (ignored)\n");
    }
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

    /* ── 准备暗环境: 只遮光, 保持工作增益 ──
     * 文档 6.15.1: 先确定 FPN 最严重的 ISO, 在该 ISO 下固定增益, 遮光采黑帧。
     * 所以这里记录当前工作曝光/增益, 转 MANUAL 冻结, 而不是压到最小。 */
    ISP_EXP_INFO_S expInfo;
    int have_exp_info = 0;

    if (!g_fpn_skip_darken) {
        if (HI_SUCCESS == HI_MPI_ISP_QueryExposureInfo(ViPipe, &expInfo)) {
            have_exp_info = 1;
            printf("[FPN] operating exposure: ExpTime=%u AGain=0x%x DGain=0x%x "
                   "ISPDGain=0x%x ISO=%u\n",
                   expInfo.u32ExpTime, expInfo.u32AGain, expInfo.u32DGain,
                   expInfo.u32ISPDGain, expInfo.u32ISO);
        } else {
            printf("[FPN] QueryExposureInfo failed, will keep AE as-is\n");
        }
        ov6946_log_exposure();
        fpn_led_off();
    } else {
        printf("[FPN] *lit-frame dump mode*: LED/exposure left untouched\n");
    }

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

    /* StartVi 会重置传感器, 用刚才记录的工作曝光/增益重新冻结 AE, 保证黑帧
     * 是在"工作增益"下采集的 (文档 6.15.1 步骤 1)。黑电平不再拉到最大 ——
     * 文档没有这个要求, 且它可能把要捕捉的列间差异一起压平。 */
    if (!g_fpn_skip_darken && have_exp_info) {
        ISP_EXPOSURE_ATTR_S exp;

        HI_MPI_ISP_GetExposureAttr(ViPipe, &exp);
        exp.enOpType = OP_TYPE_MANUAL;
        exp.stManual.enExpTimeOpType  = OP_TYPE_MANUAL;
        exp.stManual.enAGainOpType    = OP_TYPE_MANUAL;
        exp.stManual.enDGainOpType    = OP_TYPE_MANUAL;
        exp.stManual.enISPDGainOpType = OP_TYPE_MANUAL;
        exp.stManual.u32ExpTime  = expInfo.u32ExpTime;
        exp.stManual.u32AGain    = expInfo.u32AGain;
        exp.stManual.u32DGain    = expInfo.u32DGain;
        exp.stManual.u32ISPDGain = expInfo.u32ISPDGain;
        HI_MPI_ISP_SetExposureAttr(ViPipe, &exp);
        printf("[FPN] AE frozen at operating gain (ISO=%u)\n", expInfo.u32ISO);
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
        if (g_fpn_skip_darken) {
            snprintf(fn, sizeof(fn), FPN_LIT_DUMP_FILE,
                     stPubAttr.stWndRect.u32Width,
                     stPubAttr.stWndRect.u32Height, 16);
        } else {
            fpn_make_filename(fn, sizeof(fn), caliInfo.enFpnType,
                              stPubAttr.stWndRect.u32Width,
                              stPubAttr.stWndRect.u32Height, 16);
        }
        printf("[FPN] saving to %s...\n", fn);
        FILE *fp = fopen(fn, "wb");
        if (fp) {
            SAMPLE_COMM_VI_SaveFpnFile(stCaliAttr.enFpnType,
                                       &stCaliAttr.stFpnCaliFrame, fp);
            fclose(fp);
            /* 程序以 root 运行, 默认 umask 下文件是 root:root 0600,
             * 开发机上读不到。放开成 644 省去每次手动 chmod。 */
            if (chmod(fn, 0644) != 0)
                printf("[FPN] warning: chmod 0644 on %s failed\n", fn);
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
    /* 亮场采样模式下不启用校正 —— 那是纯观测, 不改设备状态 */
    if (g_fpn_status == FPN_STATUS_OK && !g_fpn_skip_darken) {
        memset(&corrInfo, 0, sizeof(corrInfo));
        corrInfo.enOpType       = OP_TYPE_AUTO;
        corrInfo.enFpnType      = caliInfo.enFpnType;
        corrInfo.u32Strength    = FPN_CORRECTION_STRENGTH;
        corrInfo.enPixelFormat  = caliInfo.enPixelFormat;
        corrInfo.enCompressMode = caliInfo.enCompressMode;

        HI_MPI_VI_DisableChn(ViPipe, ViChn);
        s32Ret = SAMPLE_COMM_VI_FpnCorrectionConfig(ViPipe, &corrInfo);
        HI_MPI_VI_EnableChn(ViPipe, ViChn);

        if (HI_SUCCESS != s32Ret) {
            printf("[FPN] enable correction failed: 0x%x\n", s32Ret);
            g_fpn_status = FPN_STATUS_FAILED;
        } else {
            printf("[FPN] correction enabled (strength=%d)\n",
                   corrInfo.u32Strength);
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
        corrInfo.u32Strength    = FPN_CORRECTION_STRENGTH;
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
    printf("[FPN] loaded calibration, correction enabled (strength=%d)\n",
           FPN_CORRECTION_STRENGTH);
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
    corrInfo.u32Strength    = FPN_CORRECTION_STRENGTH;
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
    printf("[FPN] loaded calibration (reused session), correction enabled (strength=%d)\n",
           corrInfo.u32Strength);
    return g_fpn_status;
}

/* ── 手动重试 ── */

fpn_status_t mpp_fpn_retry(VI_PIPE ViPipe)
{
    printf("[FPN] manual retry...\n");
    return mpp_fpn_calibrate(ViPipe);
}

/* ── 调试: 采一帧亮场数据 ── */

fpn_status_t mpp_fpn_dump_lit(VI_PIPE ViPipe)
{
    fpn_status_t ret;

    printf("[FPN] dumping a LIT frame (LED/exposure untouched)...\n");
    g_fpn_skip_darken = 1;
    ret = mpp_fpn_calibrate(ViPipe);
    g_fpn_skip_darken = 0;

    printf("[FPN] lit dump done, ret=%d\n", ret);
    return ret;
}

/* ── 在运行中的管线上执行一次 FPN 采样 ──
 * 切换 VI-VPSS 在线/离线模式要求 VPSS 处于停止状态, 所以必须先解绑并停掉
 * VPSS, 采样完再重建、重绑。设置页的手动校准和调试用的亮场采样共用这里,
 * 避免两处各写一份而逐渐分叉 (踩过的坑: 漏掉停 VPSS 会导致
 * SAMPLE_COMM_VI_SetParam 报 0xa0028009 而切模式失败)。
 * lit: 0 = 正常暗场校准, 1 = 亮场采样(跳过变暗步骤) */
fpn_status_t mpp_fpn_calibrate_running(int lit)
{
    video_context_t *vc = video_get_context();
    VPSS_GRP_ATTR_S grp;
    VPSS_CHN_ATTR_S chn[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL ab[VPSS_MAX_PHY_CHN_NUM];
    fpn_status_t ret;
    HI_S32 r;

    if (!vc) {
        printf("[FPN] no video context\n");
        return FPN_STATUS_FAILED;
    }

    printf("[FPN] stopping preview for %s...\n", lit ? "lit dump" : "calibration");
    vc->b_running = HI_FALSE;
    video_stop(vc);
    SAMPLE_COMM_VPSS_UnBind_VO(vc->vpss_grp, vc->vpss_chn, vc->vo_dev, vc->vo_chn);
    SAMPLE_COMM_VI_UnBind_VPSS(vc->vi_pipe, vc->vi_chn, vc->vpss_grp);
    memset(ab, 0, sizeof(ab));
    ab[vc->vpss_chn] = HI_TRUE;
    SAMPLE_COMM_VPSS_Stop(vc->vpss_grp, ab);

    ret = lit ? mpp_fpn_dump_lit(vc->vi_pipe)
              : mpp_fpn_calibrate(vc->vi_pipe);
    printf("[FPN] sampling returned %d, rebuilding VPSS...\n", ret);

    memset(&grp, 0, sizeof(grp));
    grp.stFrameRate.s32SrcFrameRate = -1;
    grp.stFrameRate.s32DstFrameRate = -1;
    grp.enDynamicRange = DYNAMIC_RANGE_SDR8;
    grp.enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    grp.u32MaxW = vc->sensor->width;
    grp.u32MaxH = vc->sensor->height;
    grp.bNrEn   = HI_FALSE;                 /* 3DNR 关闭 */
    grp.stNrAttr.enCompressMode  = COMPRESS_MODE_FRAME;
    grp.stNrAttr.enNrMotionMode  = NR_MOTION_MODE_NORMAL;

    memset(chn, 0, sizeof(chn));
    chn[vc->vpss_chn].u32Width         = vc->sensor->width;
    chn[vc->vpss_chn].u32Height        = vc->sensor->height;
    chn[vc->vpss_chn].enChnMode        = VPSS_CHN_MODE_USER;
    chn[vc->vpss_chn].enCompressMode   = COMPRESS_MODE_NONE;
    chn[vc->vpss_chn].enDynamicRange   = DYNAMIC_RANGE_SDR8;
    chn[vc->vpss_chn].enVideoFormat    = VIDEO_FORMAT_LINEAR;
    chn[vc->vpss_chn].enPixelFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    chn[vc->vpss_chn].stFrameRate.s32SrcFrameRate = (HI_S32)vc->sensor->fps;
    chn[vc->vpss_chn].stFrameRate.s32DstFrameRate = (HI_S32)vc->sensor->fps;

    memset(ab, 0, sizeof(ab));
    ab[vc->vpss_chn] = HI_TRUE;
    printf("[FPN] starting VPSS...\n");
    r = SAMPLE_COMM_VPSS_Start(vc->vpss_grp, ab, &grp, chn);
    printf("[FPN] VPSS start ret=0x%x\n", r);

    SAMPLE_COMM_VI_Bind_VPSS(vc->vi_pipe, vc->vi_chn, vc->vpss_grp);
    SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn, vc->vo_dev, vc->vo_chn);
    video_start(vc);
    printf("[FPN] video restarted\n");

    return ret;
}
