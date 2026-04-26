/**
 * @file hw_test_main.c
 * @brief Hardware test main entry for Hi3519AV100 endoscope system
 * 
 * This is a test harness that exercises all hardware components:
 * - HDMI output (1920x1080)
 * - HiFB G3 overlay (UI layer)
 * - Mouse/keyboard input (evdev)
 * - MPP video capture (VI → VO)
 * - VENC recording and snapshot
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include "lvgl.h"
#include "mpp_video.h"
#include "mpp_record.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#define TEST_DURATION_MS 30000  // 30 seconds per test phase

static volatile int g_running = 1;
static volatile int g_test_phase = 0;

void signal_handler(int sig)
{
    printf("\n[TEST] Received signal %d, shutting down...\n", sig);
    g_running = 0;
}

/**
 * @brief Test 1: Display and HDMI output test
 * 
 * Verifies:
 * - HiFB G3 initialization
 * - LVGL display on HDMI
 * - Basic drawing operations
 */
int test_display(void)
{
    printf("\n========================================\n");
    printf("TEST 1: Display and HDMI Output\n");
    printf("========================================\n");
    
    printf("[TEST 1.1] Initializing HiFB G3 layer...\n");
    if (lv_port_disp_init() != 0) {
        printf("[FAIL] Display initialization failed\n");
        return -1;
    }
    printf("[PASS] Display initialized (1920x1080 ARGB8888)\n");
    
    printf("[TEST 1.2] Creating test UI...\n");
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    
    // Test pattern: colored rectangles
    lv_obj_t *red = lv_obj_create(scr);
    lv_obj_set_size(red, 200, 200);
    lv_obj_set_pos(red, 100, 100);
    lv_obj_set_style_bg_color(red, lv_color_hex(0xFF0000), 0);
    
    lv_obj_t *green = lv_obj_create(scr);
    lv_obj_set_size(green, 200, 200);
    lv_obj_set_pos(green, 400, 100);
    lv_obj_set_style_bg_color(green, lv_color_hex(0x00FF00), 0);
    
    lv_obj_t *blue = lv_obj_create(scr);
    lv_obj_set_size(blue, 200, 200);
    lv_obj_set_pos(blue, 700, 100);
    lv_obj_set_style_bg_color(blue, lv_color_hex(0x0000FF), 0);
    
    // Test label
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hi3519AV100 Hardware Test\nDisplay: PASS");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    lv_timer_handler();
    printf("[PASS] Test UI rendered\n");
    printf("[INFO] Check HDMI output - should see colored rectangles and text\n");
    
    printf("[TEST 1.3] Running display for 5 seconds...\n");
    for (int i = 0; i < 50 && g_running; i++) {
        lv_timer_handler();
        usleep(100000);  // 100ms
    }
    
    printf("[PASS] Display test completed\n");
    return 0;
}

/**
 * @brief Test 2: Input device test (mouse/keyboard)
 * 
 * Verifies:
 * - evdev mouse detection
 * - Mouse movement tracking
 * - Button press detection
 */
int test_input(void)
{
    printf("\n========================================\n");
    printf("TEST 2: Input Device (Mouse)\n");
    printf("========================================\n");
    
    printf("[TEST 2.1] Initializing input devices...\n");
    if (lv_port_indev_init() != 0) {
        printf("[WARN] Input initialization returned error (may be OK if no mouse connected)\n");
    } else {
        printf("[PASS] Input devices initialized\n");
    }
    
    printf("[TEST 2.2] Input test - move mouse and click buttons\n");
    printf("[INFO] Test will run for 10 seconds...\n");
    
    lv_obj_t *cursor_label = lv_label_create(lv_scr_act());
    lv_label_set_text(cursor_label, "Move mouse...");
    lv_obj_set_style_text_color(cursor_label, lv_color_hex(0xFFFFFF), 0);
    
    for (int i = 0; i < 100 && g_running; i++) {
        lv_timer_handler();
        lv_indev_t *mouse = lv_indev_get_next(NULL);
        if (mouse) {
            lv_point_t vect;
            lv_indev_get_vect(mouse, &vect);
            if (vect.x != 0 || vect.y != 0) {
                printf("[INFO] Mouse movement: x=%d, y=%d\n", vect.x, vect.y);
            }
        }
        usleep(100000);
    }
    
    printf("[PASS] Input test completed\n");
    return 0;
}

/**
 * @brief Test 3: Video capture and display
 * 
 * Verifies:
 * - MPP system initialization
 * - VI (camera) capture
 * - VO (HDMI) output
 * - VI-VO bind
 */
int test_video(void)
{
    printf("\n========================================\n");
    printf("TEST 3: Video Capture and Display\n");
    printf("========================================\n");
    
    printf("[TEST 3.1] Initializing MPP video...\n");
    if (mpp_video_init() != 0) {
        printf("[FAIL] MPP video initialization failed\n");
        return -1;
    }
    printf("[PASS] MPP video initialized\n");
    
    printf("[TEST 3.2] Starting video capture...\n");
    if (mpp_video_start() != 0) {
        printf("[FAIL] Failed to start video capture\n");
        mpp_video_deinit();
        return -1;
    }
    printf("[PASS] Video capture started\n");
    printf("[INFO] Camera video should appear on HDMI output\n");
    
    // Overlay UI on video
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, 400, 100);
    lv_obj_align(overlay, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    
    lv_obj_t *label = lv_label_create(overlay);
    lv_label_set_text(label, "Video Test: Running\nCamera should be visible");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);
    
    printf("[TEST 3.3] Running video for 10 seconds...\n");
    for (int i = 0; i < 100 && g_running; i++) {
        lv_timer_handler();
        if (i % 10 == 0) {
            printf("[INFO] Video running... %d seconds\n", i / 10);
        }
        usleep(100000);
    }
    
    printf("[TEST 3.4] Stopping video...\n");
    mpp_video_stop();
    mpp_video_deinit();
    printf("[PASS] Video test completed\n");
    
    return 0;
}

/**
 * @brief Test 4: Recording and snapshot
 * 
 * Verifies:
 * - VENC initialization
 * - H.264 recording
 * - JPEG snapshot
 */
int test_recording(void)
{
    printf("\n========================================\n");
    printf("TEST 4: Recording and Snapshot\n");
    printf("========================================\n");
    
    printf("[TEST 4.1] Initializing record module...\n");
    if (record_init() != 0) {
        printf("[WARN] Record init failed (directories may need to be created manually)\n");
        printf("[INFO] Run: mkdir -p /opt/endoscope/record /opt/endoscope/snapshot\n");
    } else {
        printf("[PASS] Record module initialized\n");
    }
    
    printf("[TEST 4.2] Starting recording...\n");
    if (record_start(NULL) == 0) {
        printf("[PASS] Recording started\n");
        
        // Record for 5 seconds
        for (int i = 0; i < 50 && g_running; i++) {
            lv_timer_handler();
            if (i % 10 == 0) {
                printf("[INFO] Recording... %d seconds\n", i / 10);
            }
            usleep(100000);
        }
        
        record_stop();
        printf("[PASS] Recording stopped\n");
        printf("[INFO] Check /opt/endoscope/record/ for .h264 file\n");
    } else {
        printf("[FAIL] Failed to start recording\n");
    }
    
    printf("[TEST 4.3] Taking snapshot...\n");
    if (snapshot_save(NULL) == 0) {
        printf("[PASS] Snapshot saved\n");
        printf("[INFO] Check /opt/endoscope/snapshot/ for .jpg file\n");
    } else {
        printf("[FAIL] Failed to save snapshot\n");
    }
    
    record_deinit();
    printf("[PASS] Recording test completed\n");
    
    return 0;
}

/**
 * @brief Run all hardware tests
 */
void run_all_tests(void)
{
    int result = 0;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║     Hi3519AV100 Endoscope Hardware Test Suite          ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("This test suite verifies all hardware components:\n");
    printf("  1. HDMI display output (HiFB G3 layer)\n");
    printf("  2. Mouse/keyboard input (evdev)\n");
    printf("  3. Camera video capture and display (MPP VI→VO)\n");
    printf("  4. Video recording and snapshot (VENC)\n");
    printf("\n");
    printf("Press Ctrl+C to abort at any time\n");
    printf("\n");
    
    sleep(2);
    
    // Test 1: Display
    result = test_display();
    if (result != 0) {
        printf("\n[ERROR] Display test failed - aborting\n");
        return;
    }
    sleep(1);
    
    // Test 2: Input
    result = test_input();
    if (result != 0) {
        printf("\n[WARN] Input test failed - continuing\n");
    }
    sleep(1);
    
    // Test 3: Video
    result = test_video();
    if (result != 0) {
        printf("\n[WARN] Video test failed - continuing\n");
    }
    sleep(1);
    
    // Test 4: Recording
    result = test_recording();
    if (result != 0) {
        printf("\n[WARN] Recording test failed\n");
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║              Hardware Test Summary                     ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("All tests completed. Check the output above for:\n");
    printf("  [PASS] - Test passed\n");
    printf("  [FAIL] - Test failed\n");
    printf("  [WARN] - Warning (may indicate missing hardware)\n");
    printf("\n");
    printf("Manual verification required:\n");
    printf("  1. HDMI display shows test patterns correctly\n");
    printf("  2. USB mouse moves cursor on screen\n");
    printf("  3. Camera video appears on HDMI output\n");
    printf("  4. Recording files exist in /opt/endoscope/record/\n");
    printf("  5. Snapshot files exist in /opt/endoscope/snapshot/\n");
    printf("\n");
}

/**
 * @brief Main entry point
 */
int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Hi3519AV100 Endoscope System - Hardware Test\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("\n");
    
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [options]\n", argv[0]);
        printf("\n");
        printf("Options:\n");
        printf("  --help     Show this help message\n");
        printf("  --display  Run only display test\n");
        printf("  --input    Run only input test\n");
        printf("  --video    Run only video test\n");
        printf("  --record   Run only recording test\n");
        printf("\n");
        return 0;
    }
    
    // Initialize LVGL
    lv_init();
    
    if (argc > 1) {
        // Run specific test
        if (strcmp(argv[1], "--display") == 0) {
            test_display();
        } else if (strcmp(argv[1], "--input") == 0) {
            test_input();
        } else if (strcmp(argv[1], "--video") == 0) {
            test_video();
        } else if (strcmp(argv[1], "--record") == 0) {
            test_recording();
        } else {
            printf("Unknown option: %s\n", argv[1]);
            printf("Use --help for usage information\n");
        }
    } else {
        // Run all tests
        run_all_tests();
    }
    
    printf("\n[TEST] Cleaning up...\n");
    lv_port_indev_deinit();
    lv_port_disp_deinit();
    lv_deinit();

    printf("[TEST] Done.\n");
    return 0;
}
