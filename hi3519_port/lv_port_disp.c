#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>
#include <pthread.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "sample_comm.h"
#include "hifb.h"

#define DISP_W       1920
#define DISP_H       1080
#define G0_FB_PATH   "/dev/fb0"

volatile int g_dialog_showing = 0;
volatile int g_video_trans_enable = 1;
volatile int g_splash_showing = 1;
volatile int g_playback_mode = 0;

/* 视频透明区域 (可由 zoom 模块动态修改) */
volatile int g_varea_x = 760;
volatile int g_varea_y = 340;
volatile int g_varea_w = 400;
volatile int g_varea_h = 400;
static pthread_mutex_t g_varea_mutex = PTHREAD_MUTEX_INITIALIZER;

extern lv_display_t * lv_display_create(int32_t hor_res, int32_t ver_res);
extern void lv_display_set_flush_cb(lv_display_t * disp, void (*flush_cb)(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map));
extern void * lv_display_get_user_data(lv_display_t * disp);
extern void lv_display_set_user_data(lv_display_t * disp, void * user_data);
extern void lv_display_flush_ready(lv_display_t * disp);

static int fb_fd = -1;
static void* fb_base = NULL;
static int fix_line_length = 0;
static size_t fb_smem_len = 0;
static lv_display_t * g_disp = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static int fb_bpp = 32;

#define DRAW_BUF_SIZE (DISP_W * DISP_H / 10)
static lv_draw_buf_t draw_buf;
static uint8_t * draw_buf_mem = NULL;

static void lv_port_disp_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    if (fb_fd < 0 || fb_base == NULL) {
        lv_display_flush_ready(disp);
        return;
    }

    int32_t x = area->x1;
    int32_t y = area->y1;
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    /* Clamp to framebuffer boundaries */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > (int32_t)fb_width) w = fb_width - x;
    if (y + h > (int32_t)fb_height) h = fb_height - y;
    if (w <= 0 || h <= 0) {
        lv_display_flush_ready(disp);
        return;
    }

    if (fb_bpp == 32) {
        /* Snapshot video area under mutex for consistent read */
        int va_x, va_y, va_w, va_h;
        if (pthread_mutex_trylock(&g_varea_mutex) == 0) {
            va_x = g_varea_x; va_y = g_varea_y;
            va_w = g_varea_w; va_h = g_varea_h;
            pthread_mutex_unlock(&g_varea_mutex);
        } else {
            va_x = g_varea_x; va_y = g_varea_y;
            va_w = g_varea_w; va_h = g_varea_h;
        }

        uint32_t *src = (uint32_t *)px_map;
        for (int32_t row = 0; row < h; row++) {
            uint32_t *dst = (uint32_t *)((uint8_t*)fb_base + (y + row) * fix_line_length + x * 4);
            for (int32_t col = 0; col < w; col++) {
                int32_t abs_x = x + col;
                int32_t abs_y = y + row;
                if (g_video_trans_enable && !g_dialog_showing && !g_splash_showing &&
                    abs_x >= va_x && abs_x < va_x + va_w &&
                    abs_y >= va_y && abs_y < va_y + va_h) {
                    dst[col] = 0x0000FF00;
                } else {
                    dst[col] = src[row * w + col];
                }
            }
        }
    } else {
        uint32_t *src = (uint32_t *)px_map;
        for (int32_t row = 0; row < h; row++) {
            uint16_t *dst = (uint16_t *)((uint8_t*)fb_base + (y + row) * fix_line_length + x * 2);
            for (int32_t col = 0; col < w; col++) {
                uint32_t argb = src[row * w + col];
                uint8_t a = (argb >> 24) & 0xFF;
                uint8_t r = (argb >> 16) & 0xFF;
                uint8_t g = (argb >> 8) & 0xFF;
                uint8_t b = argb & 0xFF;
                uint16_t a1 = (a >> 7) & 0x1;
                uint16_t r5 = (r >> 3) & 0x1F;
                uint16_t g5 = (g >> 3) & 0x1F;
                uint16_t b5 = (b >> 3) & 0x1F;
                dst[col] = (a1 << 15) | (r5 << 10) | (g5 << 5) | b5;
            }
        }
    }

    lv_display_flush_ready(disp);
}

int lv_port_disp_init(void)
{
    int ret = -1;
    struct fb_var_screeninfo var_info = {0};
    struct fb_fix_screeninfo fix_info = {0};

    fb_fd = open(G0_FB_PATH, O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "[LV_PORT_DISP] Failed to open %s: %s\n", G0_FB_PATH, strerror(errno));
        return -1;
    }

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var_info) < 0) {
        fprintf(stderr, "[LV_PORT_DISP] FBIOGET_VSCREENINFO failed: %s\n", strerror(errno));
        goto cleanup_fd;
    }

    fb_bpp = var_info.bits_per_pixel;

    if (fb_bpp != 32) {
        printf("[LV_PORT_DISP] Current FB is %d bpp, reconfiguring to ARGB8888 (32bpp)...\n", fb_bpp);

        struct fb_var_screeninfo var_32bpp = var_info;
        var_32bpp.bits_per_pixel = 32;
        var_32bpp.red.length = 8;
        var_32bpp.red.offset = 16;
        var_32bpp.green.length = 8;
        var_32bpp.green.offset = 8;
        var_32bpp.blue.length = 8;
        var_32bpp.blue.offset = 0;
        var_32bpp.transp.length = 8;
        var_32bpp.transp.offset = 24;

        if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var_32bpp) < 0) {
            fprintf(stderr, "[LV_PORT_DISP] Failed to set 32bpp: %s\n", strerror(errno));
        } else if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var_info) == 0) {
            fb_bpp = var_info.bits_per_pixel;
            printf("[LV_PORT_DISP] Successfully reconfigured to %d bpp\n", fb_bpp);
        }
    }

    if (fb_bpp != 32 && fb_bpp != 16) {
        fprintf(stderr, "[LV_PORT_DISP] ERROR: fb bits_per_pixel is %d, only 16 or 32 supported\n", fb_bpp);
        goto cleanup_fd;
    }

    if (fb_bpp == 32) {
        HIFB_COLORKEY_S stColorKey = {0};
        stColorKey.bKeyEnable = HI_TRUE;
        stColorKey.u32Key = 0x0000FF00;
        if (ioctl(fb_fd, FBIOPUT_COLORKEY_HIFB, &stColorKey) < 0) {
            fprintf(stderr, "[LV_PORT_DISP] Colorkey failed: %s (video overlay may not work)\n",
                    strerror(errno));
        } else {
            printf("[LV_PORT_DISP] Colorkey enabled (green=transparent)\n");
        }
    }

    fb_width = var_info.xres;
    fb_height = var_info.yres;

    if (fb_width != DISP_W || fb_height != DISP_H) {
        fprintf(stderr, "[LV_PORT_DISP] Warning: fb resolution is %dx%d, UI is %dx%d\n",
                fb_width, fb_height, DISP_W, DISP_H);
        fprintf(stderr, "[LV_PORT_DISP] UI will be clipped to FB boundaries\n");
    }

    printf("[LV_PORT_DISP] FB config: %dx%d, %d bpp\n", fb_width, fb_height, var_info.bits_per_pixel);
    printf("[LV_PORT_DISP] FB format: R[%d:%d] G[%d:%d] B[%d:%d] A[%d:%d]\n",
           var_info.red.offset, var_info.red.length,
           var_info.green.offset, var_info.green.length,
           var_info.blue.offset, var_info.blue.length,
           var_info.transp.offset, var_info.transp.length);

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix_info) < 0) {
        fprintf(stderr, "[LV_PORT_DISP] FBIOGET_FSCREENINFO failed: %s\n", strerror(errno));
        goto cleanup_fd;
    }

    fix_line_length = fix_info.line_length;
    fb_smem_len = fix_info.smem_len;

    fb_base = mmap(NULL, fb_smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_base == MAP_FAILED) {
        fprintf(stderr, "[LV_PORT_DISP] mmap failed: %s\n", strerror(errno));
        fb_base = NULL;
        goto cleanup_fd;
    }

    memset(fb_base, 0, fix_info.smem_len);

    g_disp = lv_display_create(DISP_W, DISP_H);
    if (g_disp == NULL) {
        fprintf(stderr, "[LV_PORT_DISP] lv_display_create failed\n");
        goto cleanup_mmap;
    }

    lv_display_set_flush_cb(g_disp, lv_port_disp_flush_cb);
    lv_display_set_user_data(g_disp, (void*)(intptr_t)fb_fd);

    {
        uint32_t stride = lv_draw_buf_width_to_stride(DISP_W, LV_COLOR_FORMAT_ARGB8888);
        uint32_t buf_size = stride * (DISP_H / 10);
        draw_buf_mem = lv_malloc(buf_size);
        if (draw_buf_mem == NULL) {
            fprintf(stderr, "[LV_PORT_DISP] Failed to allocate draw buffer memory\n");
            goto cleanup_display;
        }
        lv_draw_buf_init(&draw_buf, DISP_W, DISP_H / 10, LV_COLOR_FORMAT_ARGB8888, stride, draw_buf_mem, buf_size);
    }

    lv_display_set_draw_buffers(g_disp, &draw_buf, NULL);
    lv_display_set_render_mode(g_disp, LV_DISPLAY_RENDER_MODE_PARTIAL);

    printf("[LV_PORT_DISP] Display initialized: %dx%d, fb=%s\n", DISP_W, DISP_H, G0_FB_PATH);
    return 0;

cleanup_display:
    lv_display_delete(g_disp);
    g_disp = NULL;
cleanup_mmap:
    munmap(fb_base, fb_smem_len);
    fb_base = NULL;
    fb_smem_len = 0;
cleanup_fd:
    close(fb_fd);
    fb_fd = -1;
    return ret;
}

void lv_port_disp_set_video_area(int x, int y, int w, int h)
{
    printf("[TRANS] set video area: (%d,%d %dx%d)\n", x, y, w, h);

    /* 1. 先把旧透明区域的像素恢复 (用当前UI内容填充) */
    /* 2. 更新透明区域为新值 */
    pthread_mutex_lock(&g_varea_mutex);
    g_varea_x = x;
    g_varea_y = y;
    g_varea_w = w;
    g_varea_h = h;
    pthread_mutex_unlock(&g_varea_mutex);

    /* 3. 强制整屏重绘: invalidate后触发一轮LVGL处理 */
    lv_obj_invalidate(lv_screen_active());
    lv_timer_handler_run_in_period(5); /* 给5ms处理刷新 */
}

int lv_port_disp_snapshot(void *buf, size_t buf_size, uint32_t *width, uint32_t *height, uint32_t *stride, int *bpp)
{
    if (fb_fd < 0 || fb_base == NULL) return -1;
    size_t size = fb_height * fix_line_length;
    if (buf_size < size) return -1;
    memcpy(buf, fb_base, size);
    if (width) *width = fb_width;
    if (height) *height = fb_height;
    if (stride) *stride = fix_line_length;
    if (bpp) *bpp = fb_bpp;
    return 0;
}

void lv_port_disp_deinit(void)
{
    if (g_disp) {
        lv_display_delete(g_disp);
        g_disp = NULL;
    }
    if (draw_buf_mem) {
        lv_free(draw_buf_mem);
        draw_buf_mem = NULL;
    }
    if (fb_base) {
        munmap(fb_base, fb_smem_len);
        fb_base = NULL;
        fb_smem_len = 0;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
}
