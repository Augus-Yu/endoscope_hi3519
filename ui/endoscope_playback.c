#include "endoscope_playback.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "ui_theme.h"
#include "font_manager.h"
#include "ui_helpers.h"
#include "hi3519_port/mpp_playback.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define RECORD_DIR "./endoscope/record"

static lv_obj_t * playback_screen = NULL;
static lv_obj_t * file_list = NULL;
static lv_obj_t * status_label = NULL;

static void back_btn_event(lv_event_t * e);
static void refresh_btn_event(lv_event_t * e);
static void file_click_event(lv_event_t * e);
static void refresh_list(void);

static char g_selected_path[512];

void endoscope_playback_init(void)
{
    playback_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(playback_screen, UI_COLOR_BG, 0);

    lv_obj_t * header = lv_obj_create(playback_screen);
    lv_obj_set_size(header, 800, 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(header, 0, 0);

    lv_obj_t * back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_event_cb(back_btn, back_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< 返回");
    UI_SET_FONT(back_lbl);
    lv_obj_center(back_lbl);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "录像回放");
    lv_obj_set_style_text_font(title, font_manager_get_font(), 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_center(title);

    lv_obj_t * refresh_btn = lv_btn_create(header);
    lv_obj_set_size(refresh_btn, 80, 40);
    lv_obj_align(refresh_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * refresh_lbl = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_lbl, "刷新");
    UI_SET_FONT(refresh_lbl);
    lv_obj_center(refresh_lbl);

    file_list = lv_list_create(playback_screen);
    lv_obj_set_size(file_list, 760, 380);
    lv_obj_align(file_list, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(file_list, UI_COLOR_SURFACE, 0);

    status_label = lv_label_create(playback_screen);
    lv_obj_set_style_text_color(status_label, UI_COLOR_TEXT, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void endoscope_playback_show(void)
{
    refresh_list();
    lv_scr_load(playback_screen);
}

void endoscope_playback_hide(void)
{
    if (playback_is_running())
        playback_stop();
}

static void refresh_list(void)
{
    if (file_list) lv_obj_clean(file_list);

    DIR *dir = opendir(RECORD_DIR);
    if (!dir) {
        lv_obj_t * lbl = lv_label_create(file_list);
        lv_label_set_text(lbl, "无录像文件");
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
        return;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 5) continue;
        if (strcmp(entry->d_name + len - 5, ".h264") != 0 &&
            strcmp(entry->d_name + len - 4, ".264") != 0)
            continue;

        char label[256];
        snprintf(label, sizeof(label), "[视] %s", entry->d_name);
        lv_obj_t * btn = lv_list_add_btn(file_list, NULL, label);

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", RECORD_DIR, entry->d_name);
        char *path_dup = strdup(fullpath);
        lv_obj_add_event_cb(btn, file_click_event, LV_EVENT_CLICKED, path_dup);
        count++;
    }
    closedir(dir);

    if (count == 0) {
        lv_obj_t * lbl = lv_label_create(file_list);
        lv_label_set_text(lbl, "无录像文件");
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
    }
}

static void back_btn_event(lv_event_t * e)
{
    (void)e;
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
}

static void refresh_btn_event(lv_event_t * e)
{
    (void)e;
    refresh_list();
}

static void file_click_event(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target_obj(e);
    const char *path = lv_event_get_user_data(e);
    if (!path) return;

    if (playback_is_running())
        playback_stop();

    if (playback_start(path) == 0) {
        lv_label_set_text(status_label, "正在播放...");
    } else {
        lv_label_set_text(status_label, "播放失败");
    }
}
