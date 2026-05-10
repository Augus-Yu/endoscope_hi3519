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

/* ── Frame index entry ── */
typedef struct {
    long    file_offset;    /* offset in file where this frame starts */
    int     is_keyframe;   /* 1 = IDR frame (can seek to), 0 = P frame */
} frame_index_t;

/* ── Playback state ── */
static struct {
    /* Thread control */
    pthread_t   thread;
    volatile int running;
    volatile int paused;
    char        filepath[512];
    VDEC_CHN    vdec_chn;
    HI_BOOL     started;

    /* File / frame index */
    FILE         *fp;
    uint8_t      *file_buf;
    long          file_size;
    frame_index_t *index;       /* array of frame positions */
    int           total_frames; /* total frames in file */
    volatile int  current_frame; /* frame currently being sent */

    /* Seek request (set by external caller, read by thread) */
    volatile int seek_target;   /* -1 = no seek, >=0 = frame to seek to */
} g_pb = {0};

static void *playback_thread(void *arg);

/* ── H.264 frame boundary scan ── */
static int scan_one_frame(const uint8_t *buf, int buf_len,
                          int *out_start, int *out_end)
{
    /* Find start of first frame */
    int start_pos = -1;
    int i;
    for (i = 0; i < buf_len - 4; i++) {
        if (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 1) {
            int nal_off = i + 3;
            if (i > 0 && buf[i-1] == 0) nal_off = i + 3;
            int nal_type = buf[nal_off] & 0x1F;
            if (nal_type == 1 || nal_type == 5) { start_pos = i; break; }
            if ((nal_type == 7 || nal_type == 8) && start_pos < 0)
                start_pos = i;
        }
    }
    if (start_pos < 0) {
        *out_start = 0;
        *out_end = buf_len;
        return 0; /* unknown */
    }

    /* Find end of this frame */
    int end_pos = buf_len;
    for (int j = start_pos + 3; j < buf_len - 4; j++) {
        if (buf[j] == 0 && buf[j+1] == 0 && buf[j+2] == 1) {
            int nal_off = j + 3;
            if (j > 0 && buf[j-1] == 0) nal_off = j + 3;
            int nal_type = buf[nal_off] & 0x1F;
            if (nal_type == 7 || nal_type == 8 || nal_type == 5 ||
                nal_type == 1 || nal_type == 9 || nal_type == 6) {
                if ((nal_type == 1 || nal_type == 5) &&
                    (buf[nal_off + 1] & 0x80) == 0x80) {
                    end_pos = j; break;
                } else if (nal_type != 1 && nal_type != 5) {
                    end_pos = j; break;
                }
            }
        }
    }

    *out_start = start_pos;
    *out_end   = end_pos;
    return (buf[start_pos + 3] & 0x1F) == 5 ? 1 : 0; /* 1=keyframe */
}

/* Pre-scan the H.264 file to build frame index */
static int build_frame_index(void)
{
    if (!g_pb.fp) return -1;
    fseek(g_pb.fp, 0, SEEK_SET);

    int capacity = 65536;
    g_pb.index = malloc(sizeof(frame_index_t) * capacity);
    int count = 0;

    int chunk_size = 512 * 1024; /* 512KB chunks */
    uint8_t *buf = malloc(chunk_size);
    long file_pos = 0;

    while (1) {
        fseek(g_pb.fp, file_pos, SEEK_SET);
        int read_len = fread(buf, 1, chunk_size, g_pb.fp);
        if (read_len <= 0) break;

        int offset = 0;
        while (offset < read_len) {
            int start, end, is_kf;
            int remaining = read_len - offset;
            is_kf = scan_one_frame(buf + offset, remaining, &start, &end);

            if (start == 0 && end == remaining && is_kf == 0 && remaining == chunk_size) {
                /* Frame crosses chunk boundary - read more */
                break;
            }

            if (count >= capacity) {
                capacity *= 2;
                g_pb.index = realloc(g_pb.index, sizeof(frame_index_t) * capacity);
            }

            g_pb.index[count].file_offset = file_pos + offset + start;
            g_pb.index[count].is_keyframe = is_kf;
            count++;

            offset += end;
        }

        file_pos += offset;
        if (offset == 0 || read_len < chunk_size) break;
    }

    free(buf);
    g_pb.total_frames = count;

    /* Reset file position to start */
    fseek(g_pb.fp, 0, SEEK_SET);
    printf("[PB] Frame index: %d frames\n", count);
    return 0;
}

/* ── Drain all available decoded frames to VO ── */
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

/* ── Re-init VDEC (called after seek to clear decoder state) ── */
static int reinit_vdec(void)
{
    if (g_pb.started) {
        HI_MPI_VDEC_StopRecvStream(g_pb.vdec_chn);
        HI_MPI_VDEC_DestroyChn(g_pb.vdec_chn);
        SAMPLE_COMM_VDEC_ExitVBPool();
        g_pb.started = HI_FALSE;
    }

    /* Re-init VB pool */
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

    if (SAMPLE_COMM_VDEC_InitVBPool(1, &stAttr) != HI_SUCCESS) return -1;

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

    if (HI_MPI_VDEC_CreateChn(g_pb.vdec_chn, &ca) != HI_SUCCESS) return -1;
    g_pb.started = HI_TRUE;

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

    HI_MPI_VDEC_StartRecvStream(g_pb.vdec_chn);
    return 0;
}

/* ── Seek to a specific frame index ── */
static int do_seek(int frame_idx)
{
    if (frame_idx < 0 || frame_idx >= g_pb.total_frames) return -1;
    if (!g_pb.index) return -1;

    /* Find nearest keyframe at or before the target */
    int kf_idx = frame_idx;
    while (kf_idx >= 0 && !g_pb.index[kf_idx].is_keyframe) kf_idx--;
    if (kf_idx < 0) kf_idx = 0;

    /* Drain any pending decoded frames */
    drain_all_frames();

    /* Re-init VDEC to clear decoder state */
    if (reinit_vdec() != 0) return -1;

    /* Seek file to the keyframe position */
    fseek(g_pb.fp, g_pb.index[kf_idx].file_offset, SEEK_SET);
    g_pb.current_frame = kf_idx;

    printf("[PB] Seek: frame %d -> %d (keyframe at %d, offset %ld)\n",
           frame_idx, kf_idx, kf_idx, g_pb.index[kf_idx].file_offset);
    return 0;
}

/* ── Public API ── */

int playback_start(const char *filepath)
{
    printf("[PB] === start ===\n");
    if (g_pb.running) { printf("[PB] busy\n"); return 0; }

    memset(&g_pb, 0, sizeof(g_pb));
    strncpy(g_pb.filepath, filepath, sizeof(g_pb.filepath) - 1);
    g_pb.vdec_chn = 0;
    g_pb.seek_target = -1;

    video_context_t *vc = video_get_context();

    /* Disconnect live camera */
    printf("[PB] Unbind VPSS->VO...\n");
    SAMPLE_COMM_VPSS_UnBind_VO(vc->vpss_grp, vc->vpss_chn,
                                vc->vo_dev, vc->vo_chn);

    /* Init VDEC VB pool */
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

    /* Create VDEC channel */
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
    if (HI_MPI_VDEC_CreateChn(g_pb.vdec_chn, &ca) != HI_SUCCESS) {
        printf("[PB] FAIL CreateChn\n");
        SAMPLE_COMM_VDEC_ExitVBPool();
        SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn,
                                  vc->vo_dev, vc->vo_chn);
        return -1;
    }
    g_pb.started = HI_TRUE;

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

    HI_MPI_VDEC_StartRecvStream(g_pb.vdec_chn);

    /* Open file and build frame index */
    g_pb.fp = fopen(filepath, "rb");
    if (g_pb.fp) {
        fseek(g_pb.fp, 0, SEEK_END);
        g_pb.file_size = ftell(g_pb.fp);
        build_frame_index();
    }

    g_pb.running    = 1;
    g_pb.paused     = 0;
    g_playback_mode = 1;

    pthread_create(&g_pb.thread, NULL, playback_thread, NULL);
    printf("[PB] Started (%d frames)\n", g_pb.total_frames);
    return 0;
}

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
        HI_MPI_VDEC_StopRecvStream(g_pb.vdec_chn);
        HI_MPI_VDEC_DestroyChn(g_pb.vdec_chn);
        g_pb.started = HI_FALSE;
    }
    SAMPLE_COMM_VDEC_ExitVBPool();

    if (g_pb.fp) { fclose(g_pb.fp); g_pb.fp = NULL; }
    free(g_pb.index); g_pb.index = NULL;
    free(g_pb.file_buf); g_pb.file_buf = NULL;

    SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn,
                              vc->vo_dev, vc->vo_chn);
    lv_obj_invalidate(lv_scr_act());

    printf("[PB] Stopped\n");
    return 0;
}

int playback_is_running(void) { return g_pb.running; }

void playback_pause_toggle(void)
{
    g_pb.paused = !g_pb.paused;
    printf("[PB] %s\n", g_pb.paused ? "PAUSE" : "RESUME");
}

int playback_is_paused(void) { return g_pb.paused; }

int playback_get_progress(void)
{
    if (g_pb.total_frames <= 0) return 0;
    int p = g_pb.current_frame * 1000 / g_pb.total_frames;
    if (p > 1000) p = 1000;
    return p;
}

int playback_get_total_frames(void) { return g_pb.total_frames; }

int playback_get_current_frame(void) { return g_pb.current_frame; }

int playback_seek(int permille)
{
    if (!g_pb.running || g_pb.total_frames <= 0) return -1;
    if (permille < 0) permille = 0;
    if (permille > 1000) permille = 1000;

    int target = permille * g_pb.total_frames / 1000;
    g_pb.seek_target = target;
    return 0;
}

int playback_step_forward(int frames)
{
    if (!g_pb.running || g_pb.total_frames <= 0) return -1;
    int target = g_pb.current_frame + frames;
    if (target >= g_pb.total_frames) target = g_pb.total_frames - 1;
    g_pb.seek_target = target;
    return 0;
}

int playback_step_backward(int frames)
{
    if (!g_pb.running || g_pb.total_frames <= 0) return -1;
    int target = g_pb.current_frame - frames;
    if (target < 0) target = 0;
    g_pb.seek_target = target;
    return 0;
}

/* ── send-stream thread ── */

static void *playback_thread(void *arg)
{
    (void)arg;
    if (!g_pb.fp) { g_pb.running = 0; return NULL; }

    int buf_size = 400 * 400 * 3 / 2;
    uint8_t *buf = malloc(buf_size);
    if (!buf) { g_pb.running = 0; return NULL; }

    int frame_shown = 0;

    while (g_pb.running) {
        /* Handle pause */
        if (g_pb.paused) {
            usleep(50000);
            continue;
        }

        /* Handle seek request */
        if (g_pb.seek_target >= 0) {
            do_seek(g_pb.seek_target);
            g_pb.seek_target = -1;
            continue;
        }

        /* Check EOF */
        if (g_pb.current_frame >= g_pb.total_frames) {
            printf("[PB-T] EOF\n");
            break;
        }

        /* Read frame from file */
        long pos = g_pb.index[g_pb.current_frame].file_offset;
        long next_pos = (g_pb.current_frame + 1 < g_pb.total_frames)
                        ? g_pb.index[g_pb.current_frame + 1].file_offset
                        : g_pb.file_size;
        int frame_len = (int)(next_pos - pos);

        if (frame_len <= 0 || frame_len > buf_size) {
            g_pb.current_frame++;
            continue;
        }

        fseek(g_pb.fp, pos, SEEK_SET);
        if (fread(buf, 1, frame_len, g_pb.fp) != (size_t)frame_len) break;

        VDEC_STREAM_S st;
        memset(&st, 0, sizeof(st));
        st.pu8Addr      = buf;
        st.u32Len       = frame_len;
        st.u64PTS       = g_pb.current_frame * 33333ULL;
        st.bEndOfFrame  = HI_TRUE;
        st.bEndOfStream = HI_FALSE;
        st.bDisplay     = HI_TRUE;

        HI_S32 ret;
        int retry = 0;
        do {
            ret = HI_MPI_VDEC_SendStream(g_pb.vdec_chn, &st, 50);
            if (ret == HI_ERR_VDEC_BUF_FULL) {
                drain_all_frames();
                usleep(5000); retry++;
            } else if (ret != HI_SUCCESS) {
                usleep(5000); retry++;
            }
        } while (ret != HI_SUCCESS && retry < 200 && g_pb.running);

        if (ret == HI_SUCCESS) {
            g_pb.current_frame++;
            drain_all_frames();

            if ((g_pb.current_frame % 30) == 0)
                printf("[PB-T] frame %d/%d\n",
                       g_pb.current_frame, g_pb.total_frames);

            usleep(33000);
        } else {
            printf("[PB-T] Send err 0x%x at frame %d\n",
                   ret, g_pb.current_frame);
            break;
        }
    }

    /* Final drain */
    drain_all_frames();

    free(buf);

    printf("[PB-T] playback complete (%d/%d frames)\n",
           g_pb.current_frame, g_pb.total_frames);
    playback_stop();
    return NULL;
}
