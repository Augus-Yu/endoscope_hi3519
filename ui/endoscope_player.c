#include "endoscope_player.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "ui_theme.h"
#include "font_manager.h"
#include "ui_helpers.h"
#include "hi3519_port/mpp_playback.h"
#include <stdio.h>
#include <string.h>

extern volatile int g_video_trans_enable;
volatile int g_player_pending_refresh = 0;

static lv_obj_t * player_screen = NULL;
static lv_obj_t * play_btn = NULL;
static lv_obj_t * play_label = NULL;
static lv_obj_t * progress_slider = NULL;
static lv_obj_t * time_label = NULL;
static lv_timer_t * ui_timer = NULL;
static int g_slider_dragging = 0;


static void back_btn_event(lv_event_t * e)
{
    (void)e;
    if (playback_is_running() || playback_get_current_frame() > 0) {
        playback_stop();
        playback_restore_preview();
    }
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYBACK);
}

static void play_btn_event(lv_event_t * e)
{
    (void)e;
    playback_pause_toggle();
    if (playback_is_paused()) {
        lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    } else {
        lv_label_set_text(play_label, LV_SYMBOL_PAUSE);
    }
}

static void rewind_btn_event(lv_event_t * e)
{
    (void)e;
    playback_step_backward(300); /* back ~10s at 30fps */
}

static void ffwd_btn_event(lv_event_t * e)
{
    (void)e;
    playback_step_forward(300);  /* forward ~10s at 30fps */
}

static void slider_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        g_slider_dragging = 1;
    }
    if (code == LV_EVENT_RELEASED) {
        int val = lv_slider_get_value(progress_slider);
        playback_seek(val);    /* val is 0-1000 permille */
        g_slider_dragging = 0;
    }
}

static void ui_timer_cb(lv_timer_t * tmr)
{
    (void)tmr;
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

    if (!g_slider_dragging) {
        lv_slider_set_value(progress_slider, playback_get_progress(), LV_ANIM_OFF);
    }

    int total_sec = playback_get_total_frames() / 30;
    int cur_sec   = playback_get_current_frame() / 30;
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d",
             cur_sec / 60, cur_sec % 60,
             total_sec / 60, total_sec % 60);
    lv_label_set_text(time_label, buf);
}

/* 外部: 检测 EOF, 由主循环调用 */
void endoscope_player_check_eof(void)
{
    if (!playback_is_running() && playback_get_current_frame() > 0) {
        /* 已停止但未清理 — EOF 状态, 停留在最后一帧 */
    }
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

    /* 底部控制栏: 80px, 贴底 (y=1000..1080, 刚好在视频下方) */
    lv_obj_t * ctrl_bar = lv_obj_create(player_screen);
    lv_obj_set_size(ctrl_bar, LV_PCT(100), 80);
    lv_obj_align(ctrl_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ctrl_bar, lv_color_hex(0x1a2744), 0);
    lv_obj_set_style_bg_opa(ctrl_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctrl_bar, 0, 0);
    lv_obj_set_style_pad_all(ctrl_bar, 0, 0);
    lv_obj_clear_flag(ctrl_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 进度条: 控制栏最上方 */
    progress_slider = lv_slider_create(ctrl_bar);
    lv_obj_set_size(progress_slider, LV_PCT(100), 6);
    lv_obj_align(progress_slider, LV_ALIGN_TOP_MID, 0, 0);
    lv_slider_set_range(progress_slider, 0, 1000);
    lv_slider_set_value(progress_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(progress_slider, slider_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(progress_slider, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(progress_slider, UI_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_slider, UI_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(progress_slider, 0, 0);

    /* 按钮行: 进度条下方, 填满剩余空间 */
    lv_obj_t * btn_row = lv_obj_create(ctrl_bar);
    lv_obj_set_size(btn_row, LV_PCT(100), 60);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_row, lv_color_hex(0x1a2744), 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    /* 返回 (最左) */
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

    /* << 快退 */
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

    /* || 暂停/播放 (中间) */
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

    /* >> 快进 */
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

    /* 时间标签 (最右) */
    time_label = lv_label_create(btn_row);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_label_set_text(time_label, "00:00/00:00");
    lv_obj_set_style_text_color(time_label, UI_COLOR_TEXT_SECONDARY, 0);
    UI_SET_FONT(time_label);

    ui_timer = lv_timer_create(ui_timer_cb, 250, NULL);
    lv_timer_pause(ui_timer);
}

void endoscope_player_show(void)
{
    g_video_trans_enable = 1;
    if (player_screen) {
        lv_scr_load(player_screen);
    }
    if (ui_timer) lv_timer_resume(ui_timer);
}

void endoscope_player_hide(void)
{
    if (ui_timer) lv_timer_pause(ui_timer);
}
