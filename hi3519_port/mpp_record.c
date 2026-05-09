#include "mpp_record.h"
#include "mpp_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include "sample_comm.h"
#include "hi_common.h"

#define DEFAULT_BASE  "./endoscope"

static char g_record_dir[256] = DEFAULT_BASE"/record";
static char g_snapshot_dir[256] = DEFAULT_BASE"/snapshot";

typedef struct {
    int          recording;
    int          thread_stop;
    FILE *       fp;
    pthread_t    thread;
    pthread_mutex_t mutex;
    VENC_CHN     h264_chn;
    VENC_CHN     jpeg_chn;
    HI_BOOL      h264_bound;
    HI_BOOL      h264_created;
    HI_BOOL      jpeg_bound;
    HI_BOOL      jpeg_created;
} record_ctx_t;

static record_ctx_t g_rc = {
    .recording = 0, .thread_stop = 0, .fp = NULL, .thread = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .h264_chn = 0, .jpeg_chn = 1,
    .h264_bound = HI_FALSE, .h264_created = HI_FALSE,
    .jpeg_bound = HI_FALSE, .jpeg_created = HI_FALSE,
};

static void ensure_dir(const char *path);
void generate_filename(char *buf, size_t size, const char *ext);
static void *record_worker(void *arg);
static HI_S32 unbind_and_destroy(void);

int record_init(void)
{
    SAMPLE_COMM_VENC_MemConfig();
    ensure_dir(DEFAULT_BASE);
    ensure_dir(g_record_dir);
    ensure_dir(g_snapshot_dir);
    printf("[Record] Init complete\n");
    return 0;
}

int record_start(const char *filename)
{
    (void)filename;
    pthread_mutex_lock(&g_rc.mutex);
    if (g_rc.recording) { pthread_mutex_unlock(&g_rc.mutex); return 0; }

    char fname[128];
    generate_filename(fname, sizeof(fname), ".h264");
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", g_record_dir, fname);

    g_rc.fp = fopen(fullpath, "wb");
    if (!g_rc.fp) { pthread_mutex_unlock(&g_rc.mutex); return -1; }

    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_COMM_VENC_GetGopAttr(VENC_GOPMODE_NORMALP, &stGopAttr);

    if (SAMPLE_COMM_VENC_Start(g_rc.h264_chn, PT_H264, PIC_400P,
                               SAMPLE_RC_CBR, 0, &stGopAttr) != HI_SUCCESS) {
        printf("[Record] H.264 Start failed\n");
        fclose(g_rc.fp); g_rc.fp = NULL;
        pthread_mutex_unlock(&g_rc.mutex);
        return -1;
    }
    g_rc.h264_created = HI_TRUE;

    video_context_t *vctx = video_get_context();
    if (SAMPLE_COMM_VPSS_Bind_VENC(vctx->vpss_grp, vctx->vpss_chn,
                                    g_rc.h264_chn) != HI_SUCCESS) {
        printf("[Record] VPSS Bind failed\n");
        HI_MPI_VENC_StopRecvFrame(g_rc.h264_chn);
        HI_MPI_VENC_DestroyChn(g_rc.h264_chn);
        g_rc.h264_created = HI_FALSE;
        fclose(g_rc.fp); g_rc.fp = NULL;
        pthread_mutex_unlock(&g_rc.mutex);
        return -1;
    }
    g_rc.h264_bound = HI_TRUE;

    g_rc.thread_stop = 0;
    if (pthread_create(&g_rc.thread, NULL, record_worker, NULL) != 0) {
        unbind_and_destroy();
        fclose(g_rc.fp); g_rc.fp = NULL;
        pthread_mutex_unlock(&g_rc.mutex);
        return -1;
    }

    g_rc.recording = 1;
    printf("[Record] Started -> %s\n", fullpath);
    pthread_mutex_unlock(&g_rc.mutex);
    return 0;
}

int record_stop(void)
{
    pthread_mutex_lock(&g_rc.mutex);
    if (!g_rc.recording) { pthread_mutex_unlock(&g_rc.mutex); return 0; }
    g_rc.thread_stop = 1;
    pthread_mutex_unlock(&g_rc.mutex);

    pthread_join(g_rc.thread, NULL);
    pthread_mutex_lock(&g_rc.mutex);

    if (g_rc.fp) { fflush(g_rc.fp); fclose(g_rc.fp); g_rc.fp = NULL; }
    g_rc.recording = 0;

    unbind_and_destroy();
    printf("[Record] Stopped\n");
    pthread_mutex_unlock(&g_rc.mutex);
    return 0;
}

int record_deinit(void)
{
    record_stop();
    printf("[Record] Deinit\n");
    return 0;
}

void record_set_save_base(const char *base)
{
    snprintf(g_record_dir, sizeof(g_record_dir), "%s/record", base);
    snprintf(g_snapshot_dir, sizeof(g_snapshot_dir), "%s/snapshot", base);
    ensure_dir(g_record_dir);
    ensure_dir(g_snapshot_dir);
    printf("[Record] Save base set to %s\n", base);
}

int snapshot_save(const char *filename)
{
    int result = -1;
    char fname[128];
    generate_filename(fname, sizeof(fname), ".jpg");
    char fullpath[512];
    if (filename && filename[0])
        snprintf(fullpath, sizeof(fullpath), "%s/%s", g_snapshot_dir, filename);
    else
        snprintf(fullpath, sizeof(fullpath), "%s/%s", g_snapshot_dir, fname);

    SIZE_S stSize = { .u32Width = 400, .u32Height = 400 };
    if (SAMPLE_COMM_VENC_SnapStart(g_rc.jpeg_chn, &stSize, HI_FALSE) != HI_SUCCESS) {
        printf("[Record] JPEG SnapStart failed\n");
        return -1;
    }
    g_rc.jpeg_created = HI_TRUE;

    video_context_t *vctx = video_get_context();
    if (SAMPLE_COMM_VPSS_Bind_VENC(vctx->vpss_grp, vctx->vpss_chn,
                                    g_rc.jpeg_chn) != HI_SUCCESS) {
        HI_MPI_VENC_DestroyChn(g_rc.jpeg_chn);
        g_rc.jpeg_created = HI_FALSE;
        return -1;
    }
    g_rc.jpeg_bound = HI_TRUE;

    VENC_RECV_PIC_PARAM_S recv = { .s32RecvPicNum = 1 };
    HI_MPI_VENC_StartRecvFrame(g_rc.jpeg_chn, &recv);

    VENC_CHN_STATUS_S stStat;
    memset(&stStat, 0, sizeof(stStat));
    for (int wait = 0; wait < 50; wait++) {
        HI_MPI_VENC_QueryStatus(g_rc.jpeg_chn, &stStat);
        if (stStat.u32CurPacks > 0) break;
        usleep(50000);
    }
    if (stStat.u32CurPacks == 0) {
        printf("[Record] JPEG no packs after wait\n");
        goto cleanup_jpeg;
    }

    VENC_STREAM_S stStream;
    memset(&stStream, 0, sizeof(stStream));
    stStream.u32PackCount = stStat.u32CurPacks;
    stStream.pstPack = malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
    if (!stStream.pstPack) goto cleanup_jpeg;

    HI_S32 s32Ret = HI_MPI_VENC_GetStream(g_rc.jpeg_chn, &stStream, HI_TRUE);
    if (s32Ret != HI_SUCCESS) {
        printf("[Record] JPEG GetStream failed: 0x%x\n", s32Ret);
        free(stStream.pstPack);
        goto cleanup_jpeg;
    }

    FILE *fp = fopen(fullpath, "wb");
    if (!fp) {
        HI_MPI_VENC_ReleaseStream(g_rc.jpeg_chn, &stStream);
        free(stStream.pstPack);
        goto cleanup_jpeg;
    }
    for (HI_U32 i = 0; i < stStream.u32PackCount; i++)
        fwrite(stStream.pstPack[i].pu8Addr + stStream.pstPack[i].u32Offset,
               1, stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset, fp);
    fclose(fp);

    HI_MPI_VENC_ReleaseStream(g_rc.jpeg_chn, &stStream);
    free(stStream.pstPack);

    printf("[Record] Snapshot saved: %s\n", fullpath);
    HI_MPI_VENC_StopRecvFrame(g_rc.jpeg_chn);
    result = 0;
    goto cleanup_jpeg_done;

cleanup_jpeg:
    HI_MPI_VENC_StopRecvFrame(g_rc.jpeg_chn);
cleanup_jpeg_done:
    if (g_rc.jpeg_bound) {
        video_context_t *vctx = video_get_context();
        SAMPLE_COMM_VPSS_UnBind_VENC(vctx->vpss_grp, vctx->vpss_chn, g_rc.jpeg_chn);
        g_rc.jpeg_bound = HI_FALSE;
    }
    if (g_rc.jpeg_created) {
        HI_MPI_VENC_DestroyChn(g_rc.jpeg_chn);
        g_rc.jpeg_created = HI_FALSE;
    }
    return result;
}

static void *record_worker(void *arg)
{
    (void)arg;
    VENC_STREAM_S stStream;
    VENC_STREAM_BUF_INFO_S stBufInfo;
    VENC_CHN_STATUS_S stStat;
    HI_U32 i;

    memset(&stBufInfo, 0, sizeof(stBufInfo));
    if (HI_MPI_VENC_GetStreamBufInfo(g_rc.h264_chn, &stBufInfo) != HI_SUCCESS)
        return NULL;

    int venc_fd = HI_MPI_VENC_GetFd(g_rc.h264_chn);
    if (venc_fd < 0) return NULL;

    while (1) {
        pthread_mutex_lock(&g_rc.mutex);
        int stop = g_rc.thread_stop;
        pthread_mutex_unlock(&g_rc.mutex);
        if (stop) break;

        /* Query how many packs are available */
        memset(&stStat, 0, sizeof(stStat));
        HI_S32 ret = HI_MPI_VENC_QueryStatus(g_rc.h264_chn, &stStat);
        if (ret != HI_SUCCESS || stStat.u32CurPacks == 0) {
            usleep(10000);
            continue;
        }

        /* Allocate pack array */
        memset(&stStream, 0, sizeof(stStream));
        stStream.pstPack = malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
        if (!stStream.pstPack) continue;
        stStream.u32PackCount = stStat.u32CurPacks;

        ret = HI_MPI_VENC_GetStream(g_rc.h264_chn, &stStream, HI_TRUE);
        if (ret != HI_SUCCESS) {
            free(stStream.pstPack);
            usleep(10000);
            continue;
        }

        pthread_mutex_lock(&g_rc.mutex);
        if (g_rc.fp) {
            for (i = 0; i < stStream.u32PackCount; i++)
                fwrite(stStream.pstPack[i].pu8Addr + stStream.pstPack[i].u32Offset,
                       1, stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset, g_rc.fp);
            fflush(g_rc.fp);
        }
        pthread_mutex_unlock(&g_rc.mutex);

        HI_MPI_VENC_ReleaseStream(g_rc.h264_chn, &stStream);
        free(stStream.pstPack);
    }
    return NULL;
}

static HI_S32 unbind_and_destroy(void)
{
    if (g_rc.h264_bound) {
        video_context_t *vctx = video_get_context();
        SAMPLE_COMM_VPSS_UnBind_VENC(vctx->vpss_grp, vctx->vpss_chn, g_rc.h264_chn);
        g_rc.h264_bound = HI_FALSE;
    }
    if (g_rc.h264_created) {
        HI_MPI_VENC_StopRecvFrame(g_rc.h264_chn);
        HI_MPI_VENC_DestroyChn(g_rc.h264_chn);
        g_rc.h264_created = HI_FALSE;
    }
    return 0;
}

static void ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1) mkdir(path, 0777);
}

void generate_filename(char *buf, size_t size, const char *ext)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(buf, size, "%04d%02d%02d_%02d%02d%02d%s",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, ext);
}
