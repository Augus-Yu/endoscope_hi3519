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

/* ── 网格卡片尺寸 ── */
#define CARD_W      430
#define CARD_H      320
#define THUMB_H     250
#define LABEL_H     50
#define CARD_GAP    20

/* ── 延迟缩略图加载 ── */
#define MAX_DEFER 128
static struct { lv_obj_t *img; char path[512]; } defer_q[MAX_DEFER];
static int defer_count = 0;
static lv_timer_t * defer_timer = NULL;

static lv_obj_t * playback_screen = NULL;
static lv_obj_t * grid_container = NULL;
static lv_obj_t * status_label   = NULL;

static void back_btn_event(lv_event_t * e);
static void refresh_btn_event(lv_event_t * e);
static void file_click_event(lv_event_t * e);
static void refresh_list(void);
static void defer_load_next(lv_timer_t * tmr);
static void defer_thumbnail(lv_obj_t * img, const char *fullpath);

/* ── 文件名截断 ── */
static void set_truncated_text(lv_obj_t * label, const char *text, size_t max_chars)
{
    size_t len = strlen(text);
    if (len <= max_chars) {
        lv_label_set_text(label, text);
    } else {
        char buf[128];
        size_t keep = max_chars > 3 ? max_chars - 3 : 0;
        snprintf(buf, sizeof(buf), "%.*s...", (int)keep, text);
        lv_label_set_text(label, buf);
    }
}

void endoscope_playback_init(void)
{
    playback_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(playback_screen, UI_COLOR_BG, 0);
    lv_obj_set_style_pad_all(playback_screen, 0, 0);

    /* ── 顶部标题栏 ── */
    lv_obj_t * header = lv_obj_create(playback_screen);
    lv_obj_set_size(header, LV_PCT(100), 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lv_obj_t * back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 100, 40);
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
    lv_obj_set_size(refresh_btn, 100, 40);
    lv_obj_align(refresh_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t * refresh_lbl = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_lbl, _TR("PLAYBACK_REFRESH"));
    UI_SET_FONT(refresh_lbl);
    lv_obj_center(refresh_lbl);

    /* ── 滚动容器 ── */
    lv_obj_t * scroll = lv_obj_create(playback_screen);
    lv_obj_set_size(scroll, LV_PCT(100), LV_VER_RES - 60);
    lv_obj_align(scroll, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(scroll, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, CARD_GAP, 0);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);

    /* ── 网格容器 ── */
    grid_container = lv_obj_create(scroll);
    lv_obj_set_size(grid_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(grid_container, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(grid_container, 0, 0);
    lv_obj_set_style_pad_all(grid_container, 0, 0);
    lv_obj_set_flex_flow(grid_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid_container, CARD_GAP, 0);
    lv_obj_set_style_pad_column(grid_container, CARD_GAP, 0);

    /* ── 底部状态 ── */
    status_label = lv_label_create(playback_screen);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF4444), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -5);
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

/* ── 文件类型检测 ── */
static int is_image_ext(const char *name, size_t len)
{
    return (len > 4 && strcasecmp(name + len - 4, ".jpg") == 0);
}

static int is_video_ext(const char *name, size_t len)
{
    return ((len > 5 && strcmp(name + len - 5, ".h264") == 0) ||
            (len > 4 && strcmp(name + len - 4, ".264") == 0));
}

/* ── 创建一张卡片 ── */
static lv_obj_t * create_card(lv_obj_t * parent, const char *fullpath,
                               const char *fname, int is_img)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_style_bg_color(card, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_shadow_width(card, 4, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);

    /* ── 缩略图区域 (不可点击, 点击穿透到卡片) ── */
    lv_obj_t * thumb = lv_obj_create(card);
    lv_obj_set_size(thumb, CARD_W - 16, THUMB_H);
    lv_obj_align(thumb, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_color(thumb, lv_color_hex(0x0a1628), 0);
    lv_obj_set_style_border_width(thumb, 0, 0);
    lv_obj_set_style_radius(thumb, 6, 0);
    lv_obj_set_style_pad_all(thumb, 0, 0);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(thumb, LV_OBJ_FLAG_SCROLLABLE);

    if (is_img) {
        /* 占位图标 (不可点击) */
        lv_obj_t * holder = lv_obj_create(thumb);
        lv_obj_clear_flag(holder, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(holder, 80, 80);
        lv_obj_center(holder);
        lv_obj_set_style_bg_color(holder, lv_color_hex(0x1a4728), 0);
        lv_obj_set_style_radius(holder, 10, 0);
        lv_obj_set_style_border_width(holder, 0, 0);

        lv_obj_t * ph_icon = lv_label_create(holder);
        lv_label_set_text(ph_icon, LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_font(ph_icon, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ph_icon, lv_color_hex(0x44CC66), 0);
        lv_obj_center(ph_icon);

        /* JPEG 缩略图 (不可点击, 延迟加载) */
        lv_obj_t * img = lv_image_create(thumb);
        lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
        lv_obj_center(img);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
        lv_image_set_antialias(img, false);
        defer_thumbnail(img, fullpath);
    } else {
        /* 视频占位卡片 (不可点击) */
        lv_obj_t * badge = lv_obj_create(thumb);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(badge, 80, 80);
        lv_obj_center(badge);
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x1a2744), 0);
        lv_obj_set_style_radius(badge, 10, 0);
        lv_obj_set_style_border_width(badge, 0, 0);

        lv_obj_t * icon = lv_label_create(badge);
        lv_label_set_text(icon, LV_SYMBOL_VIDEO);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x4488CC), 0);
        lv_obj_center(icon);
    }

    /* ── 类型标记: [视] / [图] ── */
    lv_obj_t * tag = lv_label_create(thumb);
    const char *tag_text = is_img ? _TR("PLAYBACK_IMG_PREFIX") : _TR("PLAYBACK_VID_PREFIX");
    lv_label_set_text(tag, tag_text);
    lv_obj_set_style_text_color(tag, lv_color_white(), 0);
    lv_obj_set_style_bg_color(tag, is_img ? lv_color_hex(0x228833) : lv_color_hex(0x225588), 0);
    lv_obj_set_style_bg_opa(tag, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(tag, 8, 0);
    lv_obj_set_style_pad_ver(tag, 4, 0);
    lv_obj_set_style_radius(tag, 6, 0);
    lv_obj_set_style_text_font(tag, font_manager_get_font(), 0);
    lv_obj_align(tag, LV_ALIGN_TOP_LEFT, 6, 6);

    /* ── 文件名 (不可点击) ── */
    lv_obj_t * label = lv_label_create(card);
    lv_obj_set_size(label, CARD_W - 16, LABEL_H);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    UI_SET_FONT(label);
    set_truncated_text(label, fname, 30);

    /* ── 点击事件: 挂在卡片上, 子控件已清除 CLICKABLE ── */
    char *path_dup = strdup(fullpath);
    lv_obj_add_event_cb(card, file_click_event, LV_EVENT_CLICKED, path_dup);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

/* ── 扫描目录 ── */
static void scan_dir(const char *dirpath, lv_obj_t *parent, int *count)
{
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        int is_img = is_image_ext(entry->d_name, len);
        int is_vid = is_video_ext(entry->d_name, len);
        if (!is_img && !is_vid) continue;

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        create_card(parent, fullpath, entry->d_name, is_img);
        (*count)++;
    }
    closedir(dir);
}

/* ── 批量加载缩略图, 每 10ms 加载 4 张 ── */
static void defer_load_next(lv_timer_t * tmr)
{
    (void)tmr;
    int batch = 4;
    for (int i = 0; i < batch && defer_count > 0; i++) {
        lv_image_set_src(defer_q[0].img, defer_q[0].path);
        memmove(&defer_q[0], &defer_q[1], (size_t)(defer_count - 1) * sizeof(defer_q[0]));
        defer_count--;
    }
    if (defer_count == 0) {
        if (defer_timer) { lv_timer_del(defer_timer); defer_timer = NULL; }
    }
}

/* ── 入队: 先显示占位, 稍后在 timer 中加载真实 JPEG ── */
static void defer_thumbnail(lv_obj_t * img, const char *fullpath)
{
    if (defer_count >= MAX_DEFER) return;
    defer_q[defer_count].img = img;
    strncpy(defer_q[defer_count].path, fullpath, sizeof(defer_q[0].path) - 1);
    defer_count++;
    if (!defer_timer) {
        defer_timer = lv_timer_create(defer_load_next, 10, NULL);
    }
}

/* ── 刷新列表 ── */
static void refresh_list(void)
{
    /* 取消旧的延迟加载 */
    if (defer_timer) { lv_timer_del(defer_timer); defer_timer = NULL; }
    defer_count = 0;

    if (grid_container) lv_obj_clean(grid_container);

    int count = 0;
    scan_dir(RECORD_DIR, grid_container, &count);
    scan_dir(SNAPSHOT_DIR, grid_container, &count);

    if (count == 0 && grid_container) {
        lv_obj_t *lbl = lv_label_create(grid_container);
        lv_label_set_text(lbl, _TR("PLAYBACK_NO_FILES"));
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
        UI_SET_FONT(lbl);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    }
}

/* ── 事件 ── */
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

    if (is_image_ext(path, len)) {
        endoscope_player_show_image(path);
        screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYER);
        return;
    }

    if (playback_is_running())
        playback_stop();

    if (playback_start(path) == 0) {
        screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYER);
    } else {
        lv_label_set_text(status_label, _TR("PLAYBACK_FAILED"));
    }
}
