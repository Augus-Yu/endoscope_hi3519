#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "hi3519_port/mpp_video.h"
#include "ui/endoscope_ui.h"
#include "../lvgl/lvgl.h"
#include "hi3519_port/lv_port_disp.h"
#include "hi3519_port/lv_port_indev.h"

#define VIDEO_X      760
#define VIDEO_Y      340
#define VIDEO_WIDTH  400
#define VIDEO_HEIGHT 400

static volatile int g_running = 1;
static volatile int g_video_started = 0;

static uint32_t get_tick_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void *keyboard_thread_fn(void *arg)
{
    (void)arg;

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    printf("[Keyboard] Monitoring stdin for 's' key (snapshot)\n");

    while (g_running) {
        char c;
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 's' || c == 'S') {
                printf("[Keyboard] 's' pressed - taking UI snapshot\n");
                endoscope_ui_snapshot_save(".");
            }
        }
        usleep(10000);
    }

    return NULL;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("========================================\n");
    printf("Hi3519AV100 Endoscope UI + Video\n");
    printf("========================================\n\n");

    printf("[1/3] Initializing LVGL and UI...\n");
    lv_init();

    if (lv_port_disp_init() != 0) {
        fprintf(stderr, "Error: Display initialization failed\n");
        lv_deinit();
        return 1;
    }

    lv_port_indev_init();
    endoscope_ui_init();

    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        usleep(20000);
    }
    printf("      UI initialized and rendered\n\n");

    printf("[2/3] Initializing MPP video system...\n");
    int sensor_connected = video_is_sensor_connected();
    if (mpp_video_init() != 0) {
        fprintf(stderr, "Error: MPP video system initialization failed\n");
        lv_port_disp_deinit();
        lv_port_indev_deinit();
        lv_deinit();
        return 1;
    }

    if (video_set_position(VIDEO_X, VIDEO_Y, VIDEO_WIDTH, VIDEO_HEIGHT) != 0) {
        fprintf(stderr, "Warning: Video position setup failed\n");
    } else {
        printf("      Video position: (%d, %d), size: %dx%d\n", VIDEO_X, VIDEO_Y, VIDEO_WIDTH, VIDEO_HEIGHT);
    }
    printf("      MPP video system initialized\n\n");

    printf("[3/3] Starting keyboard monitor and video...\n");
    pthread_t kbd_thread;
    pthread_create(&kbd_thread, NULL, keyboard_thread_fn, NULL);
    printf("      Keyboard monitor started\n");

    if (sensor_connected) {
        if (mpp_video_start() != 0) {
            fprintf(stderr, "Warning: Video start failed\n");
        } else {
            g_video_started = 1;
            printf("      Video capture started\n");
        }
    } else {
        printf("      Skipping video start (sensor not connected)\n");
    }
    printf("\n");

    printf("========================================\n");
    printf("System started!\n");
    printf("- UI on HiFB G0 layer\n");
    printf("- Video on VO layer (%d,%d %dx%d)\n", VIDEO_X, VIDEO_Y, VIDEO_WIDTH, VIDEO_HEIGHT);
    printf("- Press 's' to take snapshot, Ctrl+C to exit\n");
    printf("========================================\n\n");

    uint32_t last_tick = get_tick_ms();

    while (g_running) {
        uint32_t time_till_next = lv_timer_handler();

        uint32_t now = get_tick_ms();
        uint32_t elapsed = now - last_tick;
        if (elapsed > 0) {
            lv_tick_inc(elapsed);
            last_tick = now;
        }

        if (time_till_next > 0) {
            usleep(time_till_next * 1000);
        }
    }

    printf("\n[*] Shutting down...\n");
    g_running = 0;
    pthread_join(kbd_thread, NULL);

    if (g_video_started) {
        printf("[*] Stopping video...\n");
        mpp_video_deinit();
        g_video_started = 0;
    }

    printf("[*] Deinitializing LVGL...\n");
    lv_port_indev_deinit();
    lv_port_disp_deinit();
    lv_deinit();

    printf("[*] Cleanup complete\n");
    return 0;
}
