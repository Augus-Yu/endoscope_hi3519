#include "mpp_playback.h"
#include "mpp_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "sample_comm.h"

static struct {
    pthread_t    thread;
    int          running;
    char         filepath[512];
    VDEC_CHN     vdec_chn;
    HI_BOOL      vdec_created;
} g_pb = {
    .thread = 0, .running = 0, .filepath = {0},
    .vdec_chn = 4, .vdec_created = HI_FALSE,
};

static void *playback_thread(void *arg);

int playback_start(const char *filepath)
{
    if (g_pb.running) return 0;
    strncpy(g_pb.filepath, filepath, sizeof(g_pb.filepath) - 1);

    VDEC_CHN_ATTR_S attr;
    memset(&attr, 0, sizeof(attr));
    attr.enType          = PT_H264;
    attr.enMode          = VIDEO_MODE_STREAM;
    attr.u32PicWidth     = 400;
    attr.u32PicHeight    = 400;
    attr.u32StreamBufSize = 400 * 400 * 2;
    attr.u32FrameBufSize  = 400 * 400 * 3 / 2;
    attr.u32FrameBufCnt   = 6;
    attr.stVdecVideoAttr.u32RefFrameNum   = 2;
    attr.stVdecVideoAttr.bTemporalMvpEnable = HI_TRUE;
    attr.stVdecVideoAttr.u32TmvBufSize    = 400 * 400 / 2;

    HI_S32 ret = HI_MPI_VDEC_CreateChn(g_pb.vdec_chn, &attr);
    if (ret != HI_SUCCESS) {
        printf("[Playback] VDEC CreateChn failed: 0x%x\n", ret);
        return -1;
    }
    g_pb.vdec_created = HI_TRUE;
    printf("[Playback] VDEC chn %d created\n", g_pb.vdec_chn);

    HI_MPI_VDEC_StartRecvStream(g_pb.vdec_chn);

    g_pb.running = 1;
    if (pthread_create(&g_pb.thread, NULL, playback_thread, NULL) != 0) {
        g_pb.running = 0;
        HI_MPI_VDEC_StopRecvStream(g_pb.vdec_chn);
        HI_MPI_VDEC_DestroyChn(g_pb.vdec_chn);
        g_pb.vdec_created = HI_FALSE;
        return -1;
    }
    return 0;
}

int playback_stop(void)
{
    if (!g_pb.running) return 0;
    g_pb.running = 0;
    pthread_join(g_pb.thread, NULL);

    HI_MPI_VDEC_StopRecvStream(g_pb.vdec_chn);
    if (g_pb.vdec_created) {
        HI_MPI_VDEC_DestroyChn(g_pb.vdec_chn);
        g_pb.vdec_created = HI_FALSE;
    }
    printf("[Playback] Stopped\n");
    return 0;
}

int playback_is_running(void) { return g_pb.running; }

static void *playback_thread(void *arg)
{
    (void)arg;
    FILE *fp = fopen(g_pb.filepath, "rb");
    if (!fp) { printf("[Playback] Cannot open %s\n", g_pb.filepath); g_pb.running = 0; return NULL; }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    void *buf = malloc(file_size);
    if (!buf) { fclose(fp); g_pb.running = 0; return NULL; }
    fread(buf, 1, file_size, fp);
    fclose(fp);

    size_t offset = 0;
    const size_t chunk = 4096;
    while (g_pb.running && offset < file_size) {
        size_t len = file_size - offset;
        if (len > chunk) len = chunk;

        VDEC_STREAM_S stStream;
        memset(&stStream, 0, sizeof(stStream));
        stStream.pu8Addr      = (HI_U8 *)buf + offset;
        stStream.u32Len        = len;
        stStream.bEndOfFrame   = HI_FALSE;
        stStream.bEndOfStream  = HI_FALSE;
        stStream.bDisplay      = HI_TRUE;
        stStream.u64PTS        = 0;

        HI_S32 ret = HI_MPI_VDEC_SendStream(g_pb.vdec_chn, &stStream, 100);
        if (ret == HI_SUCCESS) offset += len;
        else usleep(5000);

        VIDEO_FRAME_INFO_S stFrame;
        ret = HI_MPI_VDEC_GetFrame(g_pb.vdec_chn, &stFrame, 0);
        if (ret == HI_SUCCESS) {
            video_context_t *vctx = video_get_context();
            HI_MPI_VO_SendFrame(vctx->vo_dev, vctx->vo_chn, &stFrame, 0);
            HI_MPI_VDEC_ReleaseFrame(g_pb.vdec_chn, &stFrame);
        }
    }

    if (offset >= file_size) {
        VDEC_STREAM_S stEnd;
        memset(&stEnd, 0, sizeof(stEnd));
        stEnd.bEndOfStream = HI_TRUE;
        HI_MPI_VDEC_SendStream(g_pb.vdec_chn, &stEnd, 100);
    }

    while (g_pb.running) {
        VIDEO_FRAME_INFO_S stFrame;
        HI_S32 ret = HI_MPI_VDEC_GetFrame(g_pb.vdec_chn, &stFrame, 100);
        if (ret != HI_SUCCESS) break;
        video_context_t *vctx = video_get_context();
        HI_MPI_VO_SendFrame(vctx->vo_dev, vctx->vo_chn, &stFrame, 0);
        HI_MPI_VDEC_ReleaseFrame(g_pb.vdec_chn, &stFrame);
    }

    free(buf);
    printf("[Playback] Finished\n");
    return NULL;
}
