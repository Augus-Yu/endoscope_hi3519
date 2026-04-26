/**
 * @file endoscope_playback.c
 * @brief 回放菜单实现
 */

#include "endoscope_playback.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "ui_theme.h"

static lv_obj_t * playback_screen = NULL;
static lv_obj_t * file_list = NULL;

static void back_btn_event(lv_event_t * e);
static void file_event(lv_event_t * e);

void endoscope_playback_init(void)
{
    playback_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(playback_screen, UI_COLOR_BG, 0);

    /* Header */
    lv_obj_t * header = lv_obj_create(playback_screen);
    lv_obj_set_size(header, 800, 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(header, 0, 0);

    /* Back button */
    lv_obj_t * back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_event_cb(back_btn, back_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< Back");
    lv_obj_center(back_lbl);

    /* Title */
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Playback");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_center(title);

    /* 文件列表 */
    file_list = lv_list_create(playback_screen);
    lv_obj_set_size(file_list, 760, 380);
    lv_obj_align(file_list, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(file_list, UI_COLOR_SURFACE, 0);

    /* 添加示例文件 */
    lv_obj_t * btn;

    btn = lv_list_add_btn(file_list, NULL, "[图] IMG_20240328_001.jpg");
    lv_obj_add_event_cb(btn, file_event, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(file_list, NULL, "[图] IMG_20240328_002.jpg");
    lv_obj_add_event_cb(btn, file_event, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(file_list, NULL, "[视] VID_20240328_001.mp4");
    lv_obj_add_event_cb(btn, file_event, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(file_list, NULL, "[图] IMG_20240327_001.jpg");
    lv_obj_add_event_cb(btn, file_event, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(file_list, NULL, "[视] VID_20240327_001.mp4");
    lv_obj_add_event_cb(btn, file_event, LV_EVENT_CLICKED, NULL);
}

void endoscope_playback_show(void)
{
    lv_scr_load(playback_screen);
}

void endoscope_playback_hide(void)
{
}

static void back_btn_event(lv_event_t * e)
{
    (void)e;
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
}

static void file_event(lv_event_t * e)
{
    (void)e;
}
