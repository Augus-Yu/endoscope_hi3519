#include "endoscope_playback.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "lang_manager.h"
#include "ui_theme.h"
#include "font_manager.h"
#include "ui_helpers.h"
#include "endoscope_player.h"
#include "hi3519_port/mpp_playback.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

extern volatile int g_video_trans_enable;

#define RECORD_DIR    "./endoscope/record"
#define SNAPSHOT_DIR "./endoscope/snapshot"

static lv_obj_t * playback_screen = NULL;
static lv_obj_t * file_list = NULL;
static lv_obj_t * status_label = NULL;

static void back_btn_event(lv_event_t * e);
static void refresh_btn_event(lv_event_t * e);
static void file_click_event(lv_event_t * e);
static void refresh_list(void);

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
    lv_label_set_text(back_lbl, _TR("PLAYBACK_BACK"));
    UI_SET_FONT(back_lbl);
    lv_obj_center(back_lbl);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, _TR("PLAYBACK_TITLE"));
    lv_obj_set_style_text_font(title, font_manager_get_font(), 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_center(title);

    lv_obj_t * refresh_btn = lv_btn_create(header);
    lv_obj_set_size(refresh_btn, 80, 40);
    lv_obj_align(refresh_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * refresh_lbl = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_lbl, _TR("PLAYBACK_REFRESH"));
    UI_SET_FONT(refresh_lbl);
    lv_obj_center(refresh_lbl);

    file_list = lv_list_create(playback_screen);
    lv_obj_set_size(file_list, 760, 380);
    lv_obj_align(file_list, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(file_list, UI_COLOR_SURFACE, 0);

    status_label = lv_label_create(playback_screen);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF4444), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void endoscope_playback_show(void)
{
    g_video_trans_enable = 0;
    refresh_list();
    lv_scr_load(playback_screen);
}

void endoscope_playback_hide(void)
{
}

static int is_image_ext(const char *name, size_t len)
{
    return (len > 4 && strcasecmp(name + len - 4, ".jpg") == 0);
}

static int is_video_ext(const char *name, size_t len)
{
    return ((len > 5 && strcmp(name + len - 5, ".h264") == 0) ||
            (len > 4 && strcmp(name + len - 4, ".264") == 0));
}

static void scan_dir(const char *dirpath, lv_obj_t *list, int *count)
{
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        int is_img  = is_image_ext(entry->d_name, len);
        int is_vid  = is_video_ext(entry->d_name, len);
        if (!is_img && !is_vid) continue;

        char label[320];
        const char *prefix = is_img ? _TR("PLAYBACK_IMG_PREFIX") : _TR("PLAYBACK_VID_PREFIX");
        snprintf(label, sizeof(label), "%s %s", prefix, entry->d_name);
        lv_obj_t *btn = lv_list_add_btn(list, NULL, label);

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        char *path_dup = strdup(fullpath);
        lv_obj_add_event_cb(btn, file_click_event, LV_EVENT_CLICKED, path_dup);
        (*count)++;
    }
    closedir(dir);
}

static void refresh_list(void)
{
    if (file_list) lv_obj_clean(file_list);

    int count = 0;

    /* 扫描录像目录 */
    scan_dir(RECORD_DIR, file_list, &count);
    /* 扫描截屏目录 */
    scan_dir(SNAPSHOT_DIR, file_list, &count);

    if (count == 0) {
        lv_obj_t *lbl = lv_label_create(file_list);
        lv_label_set_text(lbl, _TR("PLAYBACK_NO_FILES"));
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
    const char *path = lv_event_get_user_data(e);
    if (!path) return;

    size_t len = strlen(path);

    /* 图片: 进入图片查看模式 */
    if (is_image_ext(path, len)) {
        endoscope_player_show_image(path);
        screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYER);
        return;
    }

    /* 视频: 原有 MPP 播放流程 */
    if (playback_is_running())
        playback_stop();

    if (playback_start(path) == 0) {
        screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYER);
    } else {
        lv_label_set_text(status_label, _TR("PLAYBACK_FAILED"));
    }
}
