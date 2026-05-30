#include "endoscope_player.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "ui_theme.h"
#include "font_manager.h"
#include "ui_helpers.h"
#include "hi3519_port/mpp_playback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

extern volatile int g_video_trans_enable;
volatile int g_player_pending_refresh = 0;

static lv_obj_t * player_screen = NULL;
static lv_obj_t * play_btn = NULL;
static lv_obj_t * play_label = NULL;
static lv_obj_t * progress_slider = NULL;
static lv_obj_t * time_label = NULL;
static lv_timer_t * ui_timer = NULL;
static int g_slider_dragging = 0;

/* ── 图片模式状态 ── */
static int       g_is_image_mode = 0;
static lv_obj_t *g_image_obj  = NULL;
static lv_obj_t *g_img_ctrl_bar = NULL;
static lv_obj_t *g_img_name_lbl = NULL;
static lv_obj_t *g_img_count_lbl = NULL;
static lv_obj_t *g_img_vid_ctrls = NULL;

static char     g_image_dir[512];
static char     g_image_path[512];
static int      g_image_index  = -1;
static char   **g_image_files  = NULL;
static int      g_image_count  = 0;

/* ── 前向声明 ── */
static void back_btn_event(lv_event_t * e);
static void play_btn_event(lv_event_t * e);
static void rewind_btn_event(lv_event_t * e);
static void ffwd_btn_event(lv_event_t * e);
static void slider_event_cb(lv_event_t * e);
static void ui_timer_cb(lv_timer_t * tmr);
static void img_prev_btn_event(lv_event_t * e);
static void img_next_btn_event(lv_event_t * e);
static void free_image_list(void);
static int  build_image_list(const char *first_path);
static void load_image(const char *path);
static void update_img_info(void);

/* ── 图片文件列表 ── */
static int name_cmp(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static int is_jpg_ext(const char *name) {
    size_t len = strlen(name);
    return (len > 4 && strcasecmp(name + len - 4, ".jpg") == 0);
}

static int build_image_list(const char *first_path)
{
    free_image_list();
    char dir[512];
    strncpy(dir, first_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *sep = strrchr(dir, '/');
    if (!sep) return -1;
    *sep = '\0';
    strncpy(g_image_dir, dir, sizeof(g_image_dir) - 1);

    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_type != DT_REG && e->d_type != DT_UNKNOWN) continue;
        if (is_jpg_ext(e->d_name)) n++;
    }
    if (n == 0) { closedir(d); return -1; }
    g_image_files = calloc(n, sizeof(char *));
    if (!g_image_files) { closedir(d); return -1; }
    rewinddir(d);
    int idx = 0;
    while ((e = readdir(d)) != NULL && idx < n) {
        if (e->d_type != DT_REG && e->d_type != DT_UNKNOWN) continue;
        if (is_jpg_ext(e->d_name))
            g_image_files[idx++] = strdup(e->d_name);
    }
    g_image_count = idx;
    closedir(d);
    qsort(g_image_files, g_image_count, sizeof(char *), name_cmp);

    const char *fname = strrchr(first_path, '/');
    fname = fname ? fname + 1 : first_path;
    g_image_index = -1;
    for (int i = 0; i < g_image_count; i++) {
        if (strcmp(g_image_files[i], fname) == 0) { g_image_index = i; break; }
    }
    return (g_image_index >= 0) ? 0 : -1;
}

static void free_image_list(void)
{
    if (g_image_files) {
        for (int i = 0; i < g_image_count; i++) free(g_image_files[i]);
        free(g_image_files);
        g_image_files = NULL;
    }
    g_image_count = 0;
    g_image_index = -1;
}

/* ── 通过 LVGL FS 驱动 + TJPGD 加载 JPEG ── */
static void load_image(const char *path)
{
    if (!g_image_obj) return;
    strncpy(g_image_path, path, sizeof(g_image_path) - 1);

    lv_image_set_src(g_image_obj, path);

    printf("[Player] loading: %s\n", path);
    update_img_info();
}

static void update_img_info(void)
{
    if (g_img_name_lbl && g_image_index >= 0)
        lv_label_set_text(g_img_name_lbl, g_image_files[g_image_index]);
    if (g_img_count_lbl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d/%d", g_image_index + 1, g_image_count);
        lv_label_set_text(g_img_count_lbl, buf);
    }
}

static void img_prev_btn_event(lv_event_t * e)
{
    (void)e;
    if (!g_is_image_mode || g_image_count <= 1) return;
    int idx = g_image_index - 1;
    if (idx < 0) idx = g_image_count - 1;
    char full[640];
    snprintf(full, sizeof(full), "%s/%s", g_image_dir, g_image_files[idx]);
    g_image_index = idx;
    load_image(full);
}

static void img_next_btn_event(lv_event_t * e)
{
    (void)e;
    if (!g_is_image_mode || g_image_count <= 1) return;
    int idx = g_image_index + 1;
    if (idx >= g_image_count) idx = 0;
    char full[640];
    snprintf(full, sizeof(full), "%s/%s", g_image_dir, g_image_files[idx]);
    g_image_index = idx;
    load_image(full);
}

void endoscope_player_show_image(const char *path)
{
    if (playback_is_running()) {
        playback_stop();
        playback_restore_preview();
    }
    if (build_image_list(path) != 0) {
        printf("[Player] failed to build image list for %s\n", path);
        return;
    }
    g_is_image_mode = 1;
    strncpy(g_image_path, path, sizeof(g_image_path) - 1);
}

/* ── 返回 ── */
static void back_btn_event(lv_event_t * e)
{
    (void)e;
    if (g_is_image_mode) {
        g_is_image_mode = 0;
        free_image_list();
        g_image_path[0] = '\0';
    }
    if (playback_is_running()) {
        playback_stop();
        playback_restore_preview();
    }
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYBACK);
}

static void play_btn_event(lv_event_t * e)
{
    (void)e;
    playback_pause_toggle();
    lv_label_set_text(play_label, playback_is_paused() ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}

static void rewind_btn_event(lv_event_t * e)
{ (void)e; playback_step_backward(300); }

static void ffwd_btn_event(lv_event_t * e)
{ (void)e; playback_step_forward(300); }

static void slider_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) g_slider_dragging = 1;
    if (code == LV_EVENT_RELEASED) {
        playback_seek(lv_slider_get_value(progress_slider));
        g_slider_dragging = 0;
    }
}

static void ui_timer_cb(lv_timer_t * tmr)
{
    (void)tmr;
    if (g_is_image_mode) return;
    if (g_player_pending_refresh) {
        g_player_pending_refresh = 0;
        lv_obj_invalidate(lv_scr_act());
    }
    if (!playback_is_running()) {
        lv_label_set_text(play_label, LV_SYMBOL_PLAY);
        lv_slider_set_value(progress_slider, 1000, LV_ANIM_OFF);
        lv_label_set_text(time_label, "100%");
        return;
    }
    if (!g_slider_dragging)
        lv_slider_set_value(progress_slider, playback_get_progress(), LV_ANIM_OFF);
    int total_sec = playback_get_total_frames() / 30;
    int cur_sec   = playback_get_current_frame() / 30;
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d",
             cur_sec / 60, cur_sec % 60, total_sec / 60, total_sec % 60);
    lv_label_set_text(time_label, buf);
}

void endoscope_player_check_eof(void)
{
}

/* ── Screen init ── */
void endoscope_player_init(void)
{
    if (player_screen) return;

    player_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(player_screen, lv_color_hex(0x0a1628), 0);
    lv_obj_set_style_bg_opa(player_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(player_screen, 0, 0);
    lv_obj_clear_flag(player_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* 图片显示 (默认隐藏) */
    g_image_obj = lv_image_create(player_screen);
    lv_obj_set_size(g_image_obj, LV_PCT(100), 920);
    lv_obj_align(g_image_obj, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(g_image_obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_image_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(g_image_obj, 20, 0);
    lv_obj_add_flag(g_image_obj, LV_OBJ_FLAG_HIDDEN);

    /* 视频控制栏 */
    lv_obj_t * vid_ctrl_bar = lv_obj_create(player_screen);
    lv_obj_set_size(vid_ctrl_bar, LV_PCT(100), 80);
    lv_obj_align(vid_ctrl_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(vid_ctrl_bar, lv_color_hex(0x1a2744), 0);
    lv_obj_set_style_bg_opa(vid_ctrl_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vid_ctrl_bar, 0, 0);
    lv_obj_set_style_pad_all(vid_ctrl_bar, 0, 0);
    lv_obj_clear_flag(vid_ctrl_bar, LV_OBJ_FLAG_SCROLLABLE);
    g_img_vid_ctrls = vid_ctrl_bar;

    progress_slider = lv_slider_create(vid_ctrl_bar);
    lv_obj_set_size(progress_slider, LV_PCT(100), 6);
    lv_obj_align(progress_slider, LV_ALIGN_TOP_MID, 0, 0);
    lv_slider_set_range(progress_slider, 0, 1000);
    lv_slider_set_value(progress_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(progress_slider, slider_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(progress_slider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(progress_slider, UI_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_slider, UI_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(progress_slider, 0, 0);

    lv_obj_t * btn_row = lv_obj_create(vid_ctrl_bar);
    lv_obj_set_size(btn_row, LV_PCT(100), 60);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_row, lv_color_hex(0x1a2744), 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * back_btn = lv_btn_create(btn_row);
    lv_obj_set_size(back_btn, 60, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x5a6a7a), 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_add_event_cb(back_btn, back_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(back_lbl);

    lv_obj_t * rw_btn = lv_btn_create(btn_row);
    lv_obj_set_size(rw_btn, 52, 40);
    lv_obj_align(rw_btn, LV_ALIGN_CENTER, -100, 0);
    lv_obj_set_style_bg_color(rw_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(rw_btn, 8, 0);
    lv_obj_add_event_cb(rw_btn, rewind_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * rw_lbl = lv_label_create(rw_btn);
    lv_label_set_text(rw_lbl, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(rw_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(rw_lbl);

    play_btn = lv_btn_create(btn_row);
    lv_obj_set_size(play_btn, 56, 40);
    lv_obj_align(play_btn, LV_ALIGN_CENTER, -30, 0);
    lv_obj_set_style_bg_color(play_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(play_btn, 20, 0);
    lv_obj_add_event_cb(play_btn, play_btn_event, LV_EVENT_CLICKED, NULL);
    play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(play_label, lv_color_white(), 0);
    lv_obj_center(play_label);

    lv_obj_t * ff_btn = lv_btn_create(btn_row);
    lv_obj_set_size(ff_btn, 52, 40);
    lv_obj_align(ff_btn, LV_ALIGN_CENTER, 40, 0);
    lv_obj_set_style_bg_color(ff_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(ff_btn, 8, 0);
    lv_obj_add_event_cb(ff_btn, ffwd_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * ff_lbl = lv_label_create(ff_btn);
    lv_label_set_text(ff_lbl, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(ff_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(ff_lbl);

    time_label = lv_label_create(btn_row);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_label_set_text(time_label, "00:00/00:00");
    lv_obj_set_style_text_color(time_label, UI_COLOR_TEXT_SECONDARY, 0);
    UI_SET_FONT(time_label);

    /* ── 图片控制栏 (默认隐藏) ── */
    g_img_ctrl_bar = lv_obj_create(player_screen);
    lv_obj_set_size(g_img_ctrl_bar, LV_PCT(100), 160);
    lv_obj_align(g_img_ctrl_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(g_img_ctrl_bar, lv_color_hex(0x1a2744), 0);
    lv_obj_set_style_bg_opa(g_img_ctrl_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_img_ctrl_bar, 0, 0);
    lv_obj_set_style_pad_all(g_img_ctrl_bar, 0, 0);
    lv_obj_clear_flag(g_img_ctrl_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_img_ctrl_bar, LV_OBJ_FLAG_HIDDEN);

    g_img_name_lbl = lv_label_create(g_img_ctrl_bar);
    lv_obj_align(g_img_name_lbl, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(g_img_name_lbl, "");
    lv_obj_set_style_text_color(g_img_name_lbl, UI_COLOR_TEXT, 0);
    UI_SET_FONT(g_img_name_lbl);

    g_img_count_lbl = lv_label_create(g_img_ctrl_bar);
    lv_obj_align(g_img_count_lbl, LV_ALIGN_TOP_MID, 0, 40);
    lv_label_set_text(g_img_count_lbl, "");
    lv_obj_set_style_text_color(g_img_count_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    UI_SET_FONT(g_img_count_lbl);

    lv_obj_t * img_btn_row = lv_obj_create(g_img_ctrl_bar);
    lv_obj_set_size(img_btn_row, LV_PCT(100), 60);
    lv_obj_align(img_btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(img_btn_row, lv_color_hex(0x1a2744), 0);
    lv_obj_set_style_border_width(img_btn_row, 0, 0);
    lv_obj_set_style_pad_all(img_btn_row, 0, 0);
    lv_obj_clear_flag(img_btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * img_back = lv_btn_create(img_btn_row);
    lv_obj_set_size(img_back, 60, 40);
    lv_obj_align(img_back, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(img_back, lv_color_hex(0x5a6a7a), 0);
    lv_obj_set_style_radius(img_back, 8, 0);
    lv_obj_add_event_cb(img_back, back_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * ib_lbl = lv_label_create(img_back);
    lv_label_set_text(ib_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(ib_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(ib_lbl);

    lv_obj_t * prev_btn = lv_btn_create(img_btn_row);
    lv_obj_set_size(prev_btn, 80, 40);
    lv_obj_align(prev_btn, LV_ALIGN_CENTER, -60, 0);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(prev_btn, 8, 0);
    lv_obj_add_event_cb(prev_btn, img_prev_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * pv_lbl = lv_label_create(prev_btn);
    lv_label_set_text(pv_lbl, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(pv_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(pv_lbl);

    lv_obj_t * next_btn = lv_btn_create(img_btn_row);
    lv_obj_set_size(next_btn, 80, 40);
    lv_obj_align(next_btn, LV_ALIGN_CENTER, 60, 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(next_btn, 8, 0);
    lv_obj_add_event_cb(next_btn, img_next_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * nx_lbl = lv_label_create(next_btn);
    lv_label_set_text(nx_lbl, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(nx_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(nx_lbl);

    ui_timer = lv_timer_create(ui_timer_cb, 250, NULL);
    lv_timer_pause(ui_timer);
}

void endoscope_player_show(void)
{
    if (!player_screen) return;

    if (g_is_image_mode) {
        g_video_trans_enable = 0;
        if (g_img_vid_ctrls) lv_obj_add_flag(g_img_vid_ctrls, LV_OBJ_FLAG_HIDDEN);
        if (g_img_ctrl_bar)  lv_obj_clear_flag(g_img_ctrl_bar, LV_OBJ_FLAG_HIDDEN);
        if (g_image_obj)     lv_obj_clear_flag(g_image_obj, LV_OBJ_FLAG_HIDDEN);
        load_image(g_image_path);
    } else {
        g_video_trans_enable = 1;
        if (g_img_vid_ctrls) lv_obj_clear_flag(g_img_vid_ctrls, LV_OBJ_FLAG_HIDDEN);
        if (g_img_ctrl_bar)  lv_obj_add_flag(g_img_ctrl_bar, LV_OBJ_FLAG_HIDDEN);
        if (g_image_obj)     lv_obj_add_flag(g_image_obj, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    }

    lv_scr_load(player_screen);
    if (ui_timer) lv_timer_resume(ui_timer);
}

void endoscope_player_hide(void)
{
    if (ui_timer) lv_timer_pause(ui_timer);
}
