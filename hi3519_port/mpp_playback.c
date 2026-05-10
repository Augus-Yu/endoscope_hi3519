#include "mpp_playback.h"
#include "mpp_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include "sample_comm.h"
#include "hi_buffer.h"
#include "lvgl.h"

extern volatile int g_playback_mode;
extern volatile int g_video_trans_enable;

static struct {
    pthread_t thread;
    int       running;
    char      filepath[512];
    VDEC_CHN  vdec_chn;
    HI_BOOL   started;
} g_pb = {0};

static void *playback_thread(void *arg);

/* ── H.264 frame boundary scan ── */
static int read_one_h264_frame(FILE *fp, uint8_t *buf, int buf_size)
{
    int read_len = fread(buf, 1, buf_size, fp);
    if (read_len <= 0) return 0;

    int start_pos = -1;
    int i;
    for (i = 0; i < read_len - 4; i++) {
        if (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 1) {
            int nal_off = i + 3;
            if (i > 0 && buf[i-1] == 0) {
                nal_off = i + 3;
            }
            int nal_type = buf[nal_off] & 0x1F;
            if (nal_type == 1 || nal_type == 5) {
                start_pos = i;
                break;
            }
            if (nal_type == 7 || nal_type == 8) {
                if (start_pos < 0) start_pos = i;
            }
        }
    }
    if (start_pos < 0) return read_len;

    int end_pos = read_len;
    for (int j = start_pos + 3; j < read_len - 4; j++) {
        if (buf[j] == 0 && buf[j+1] == 0 && buf[j+2] == 1) {
            int nal_off = j + 3;
            if (j > 0 && buf[j-1] == 0) {
                nal_off = j + 3;
            }
            int nal_type = buf[nal_off] & 0x1F;
            if (nal_type == 7 || nal_type == 8 ||
                nal_type == 5 || nal_type == 1 ||
                nal_type == 9 || nal_type == 6) {
                if (nal_type == 1 || nal_type == 5) {
                    if ((buf[nal_off + 1] & 0x80) == 0x80) {
                        end_pos = j;
                        break;
                    }
                } else {
                    end_pos = j;
                    break;
                }
            }
        }
    }

    fseek(fp, start_pos + (end_pos - start_pos) - read_len, SEEK_CUR);
    return end_pos;
}

/* ── playback_start ── */
int playback_start(const char *filepath)
{
    printf("[PB] === start ===\n");
    if (g_pb.running) { printf("[PB] busy\n"); return 0; }

    memset(&g_pb, 0, sizeof(g_pb));
    strncpy(g_pb.filepath, filepath, sizeof(g_pb.filepath) - 1);
    g_pb.vdec_chn = 0;

    video_context_t *vc = video_get_context();

    /* 1. Disconnect live camera display */
    printf("[PB] Unbind VPSS→VO...\n");
    SAMPLE_COMM_VPSS_UnBind_VO(vc->vpss_grp, vc->vpss_chn,
                                vc->vo_dev, vc->vo_chn);

    /* 2. Init VDEC VB pool with SDK helper */
    SAMPLE_VDEC_ATTR stAttr;
    memset(&stAttr, 0, sizeof(stAttr));
    stAttr.enType                       = PT_H264;
    stAttr.u32Width                     = 400;
    stAttr.u32Height                    = 400;
    stAttr.enMode                       = VIDEO_MODE_FRAME;
    stAttr.stSapmleVdecVideo.enDecMode  = VIDEO_DEC_MODE_IP;
    stAttr.stSapmleVdecVideo.enBitWidth = DATA_BITWIDTH_8;
    stAttr.stSapmleVdecVideo.u32RefFrameNum = 2;
    stAttr.u32DisplayFrameNum           = 2;
    stAttr.u32FrameBufCnt               = 5;

    printf("[PB] InitVBPool...\n");
    if (SAMPLE_COMM_VDEC_InitVBPool(1, &stAttr) != HI_SUCCESS) {
        printf("[PB] FAIL InitVBPool\n");
        SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn,
                                  vc->vo_dev, vc->vo_chn);
        return -1;
    }

    /* 3. Create VDEC channel manually with LARGER stream buffer.
     * SAMPLE_COMM_VDEC_Start uses u32StreamBufSize = width*height (160K)
     * which is too small. We use 400*400*6 = 960K. */
    VDEC_CHN_ATTR_S ca;
    memset(&ca, 0, sizeof(ca));
    ca.enType                           = PT_H264;
    ca.enMode                           = VIDEO_MODE_FRAME;
    ca.u32PicWidth                      = 400;
    ca.u32PicHeight                     = 400;
    ca.u32StreamBufSize                 = 400 * 400 * 6;
    ca.u32FrameBufCnt                   = 5;
    ca.u32FrameBufSize                  = VDEC_GetPicBufferSize(
        PT_H264, 400, 400,
        PIXEL_FORMAT_YVU_SEMIPLANAR_420,
        DATA_BITWIDTH_8, 0);
    ca.stVdecVideoAttr.u32RefFrameNum   = 2;
    ca.stVdecVideoAttr.bTemporalMvpEnable = 0;

    printf("[PB] CreateChn...\n");
    HI_S32 ret = HI_MPI_VDEC_CreateChn(g_pb.vdec_chn, &ca);
    if (ret != HI_SUCCESS) {
        printf("[PB] FAIL CreateChn 0x%x\n", ret);
        SAMPLE_COMM_VDEC_ExitVBPool();
        SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn,
                                  vc->vo_dev, vc->vo_chn);
        return -1;
    }
    g_pb.started = HI_TRUE;

    /* 4. Set channel params (same as SDK SAMPLE_COMM_VDEC_Start) */
    VDEC_CHN_PARAM_S chnParam;
    memset(&chnParam, 0, sizeof(chnParam));
    if (HI_MPI_VDEC_GetChnParam(g_pb.vdec_chn, &chnParam) == HI_SUCCESS) {
        chnParam.stVdecVideoParam.enDecMode       = VIDEO_DEC_MODE_IP;
        chnParam.stVdecVideoParam.enCompressMode  = COMPRESS_MODE_NONE;
        chnParam.stVdecVideoParam.enVideoFormat   = VIDEO_FORMAT_LINEAR;
        chnParam.stVdecVideoParam.enOutputOrder   = VIDEO_OUTPUT_ORDER_DEC;
        chnParam.u32DisplayFrameNum               = 2;
        HI_MPI_VDEC_SetChnParam(g_pb.vdec_chn, &chnParam);
    }

    printf("[PB] StartRecvStream...\n");
    HI_MPI_VDEC_StartRecvStream(g_pb.vdec_chn);

    g_pb.running    = 1;
    g_playback_mode = 1;

    pthread_create(&g_pb.thread, NULL, playback_thread, NULL);
    printf("[PB] Started\n");
    return 0;
}

/* ── playback_stop ── */
int playback_stop(void)
{
    g_playback_mode = 0;
    g_video_trans_enable = 1;

    if (g_pb.thread) {
        g_pb.running = 0;
        pthread_join(g_pb.thread, NULL);
        g_pb.thread = 0;
    }

    video_context_t *vc = video_get_context();

    if (g_pb.started) {
        printf("[PB] StopRecvStream...\n");
        HI_MPI_VDEC_StopRecvStream(g_pb.vdec_chn);
        printf("[PB] DestroyChn...\n");
        HI_MPI_VDEC_DestroyChn(g_pb.vdec_chn);
        g_pb.started = HI_FALSE;
    }

    SAMPLE_COMM_VDEC_ExitVBPool();

    /* Restore live camera display */
    printf("[PB] Restore VPSS→VO...\n");
    SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn,
                              vc->vo_dev, vc->vo_chn);

    /* Invalidate the active screen to force LVGL full redraw on next tick.
     * During playback the entire screen was transparent green (colorkey),
     * after stopping we must ensure the UI is re-rendered. */
    lv_obj_invalidate(lv_scr_act());

    printf("[PB] Stopped\n");
    return 0;
}

int playback_is_running(void) { return g_pb.running; }

/* ── Drain ALL available decoded frames to VO ── */
/* Returns the number of frames sent to VO. */
static int drain_all_frames(void)
{
    int cnt = 0;
    while (1) {
        VIDEO_FRAME_INFO_S f;
        HI_S32 ret = HI_MPI_VDEC_GetFrame(g_pb.vdec_chn, &f, 0);
        if (ret != HI_SUCCESS) break;
        video_context_t *vc = video_get_context();
        HI_MPI_VO_SendFrame(vc->vo_dev, vc->vo_chn, &f, 0);
        HI_MPI_VDEC_ReleaseFrame(g_pb.vdec_chn, &f);
        cnt++;
    }
    return cnt;
}

/* ── send-stream thread ── */
static void *playback_thread(void *arg)
{
    (void)arg;
    FILE *fp = fopen(g_pb.filepath, "rb");
    if (!fp) { printf("[PB-T] FAIL open\n"); g_pb.running = 0; return NULL; }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("[PB-T] file=%ld bytes\n", file_size);

    int buf_size = 400 * 400 * 3 / 2;
    uint8_t *buf = malloc(buf_size);
    if (!buf) { fclose(fp); g_pb.running = 0; return NULL; }

    int frame_sent   = 0;
    int frame_shown  = 0;
    HI_U64 u64PTS    = 0;

    while (g_pb.running) {
        int frame_len = read_one_h264_frame(fp, buf, buf_size);
        if (frame_len <= 0) {
            printf("[PB-T] EOF (sent=%d shown=%d)\n", frame_sent, frame_shown);
            break;
        }

        VDEC_STREAM_S st;
        memset(&st, 0, sizeof(st));
        st.pu8Addr      = buf;
        st.u32Len       = frame_len;
        st.u64PTS       = u64PTS;
        st.bEndOfFrame  = HI_TRUE;
        st.bEndOfStream = HI_FALSE;
        st.bDisplay     = HI_TRUE;

        HI_S32 ret;
        int retry = 0;
        do {
            ret = HI_MPI_VDEC_SendStream(g_pb.vdec_chn, &st, 50);

            if (ret == HI_ERR_VDEC_BUF_FULL) {
                drain_all_frames();
                usleep(5000);
                retry++;
            } else if (ret != HI_SUCCESS) {
                usleep(5000);
                retry++;
            }
        } while (ret != HI_SUCCESS && retry < 200 && g_pb.running);

        if (ret == HI_SUCCESS) {
            u64PTS += 33333;
            frame_sent++;

            drain_all_frames();

            if ((frame_sent % 30) == 0)
                printf("[PB-T] sent=%d\n", frame_sent);

            usleep(33000);
        } else {
            printf("[PB-T] Send err 0x%x after %d frames (retry=%d)\n",
                   ret, frame_sent, retry);
            break;
        }
    }

    /* Drain all remaining decoded frames */
    printf("[PB-T] final drain...\n");
    int drained = 0;
    while (g_pb.running) {
        VIDEO_FRAME_INFO_S f;
        HI_S32 ret = HI_MPI_VDEC_GetFrame(g_pb.vdec_chn, &f, 100);
        if (ret != HI_SUCCESS) break;
        video_context_t *vc = video_get_context();
        HI_MPI_VO_SendFrame(vc->vo_dev, vc->vo_chn, &f, 0);
        HI_MPI_VDEC_ReleaseFrame(g_pb.vdec_chn, &f);
        drained++;
    }
    printf("[PB-T] done: sent=%d drained=%d\n", frame_sent, drained);

    free(buf);
    fclose(fp);

    printf("[PB-T] playback complete\n");
    playback_stop();
    return NULL;
}
