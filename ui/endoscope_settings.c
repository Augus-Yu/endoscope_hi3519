/**
 * @file endoscope_settings.c
 * @brief 设置菜单实现 - 二级菜单设计
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "endoscope_settings.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "endoscope_dialogs.h"
#include "font_manager.h"
#include "lang_manager.h"
#include "ui_helpers.h"
#include "hi3519_port/mpp_fpn.h"
#include "hi3519_port/mpp_video.h"

/*********************
 *      DEFINES
 *********************/
#define SETTING_COLOR_BG       UI_COLOR_BG
#define SETTING_COLOR_SURFACE  UI_COLOR_SURFACE
#define SETTING_COLOR_TEXT     UI_COLOR_TEXT
#define SETTING_COLOR_ACCENT   UI_COLOR_ACCENT
#define SETTING_COLOR_GRAY     lv_color_hex(0x4a5568)
#define SETTING_COLOR_SELECTED UI_COLOR_ACCENT
#define TOP_BAR_HEIGHT         120
#define LEFT_MENU_WIDTH        360
#define NAV_ITEM_HEIGHT        96

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * settings_screen = NULL;
static lv_obj_t * nav_menu = NULL;
static lv_obj_t * content_area = NULL;
static lv_obj_t * nav_btns[4] = {NULL};
static lv_obj_t * content_pages[4] = {NULL};
static lv_obj_t * key_row_btns[4][3] = {{NULL}};  /* 4行，每行3个按钮 */
static int key_row_selected[4] = {0, 0, 0, 0};   /* 每行当前选中的按钮索引 */
static lv_obj_t * lang_btns[2] = {NULL};         /* 语言切换按钮 (0=中文, 1=英文) */
static int current_lang = 0;                      /* 当前语言: 0=中文, 1=英文 */
static int current_page = 0;
static lv_obj_t * time_input = NULL;             /* 时间输入框引用 */
static lv_timer_t * time_timer = NULL;           /* 时间更新定时器 */

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void create_top_bar(void);
static void create_nav_menu(void);
static void create_content_area(void);
static void create_power_page(void);
static void create_keys_page(void);
static void create_system_page(void);
static void create_about_page(void);
static void nav_btn_event_cb(lv_event_t * e);
static void back_btn_event_cb(lv_event_t * e);
static void power_btn_event_cb(lv_event_t * e);
static void key_config_btn_event_cb(lv_event_t * e);
static void system_btn_event_cb(lv_event_t * e);
static void lang_btn_event_cb(lv_event_t * e);
static void do_fpn_calibration(void);
static void do_fpn_confirm_cb(bool confirmed);

/* 工厂重置确认回调 */
static void factory_reset_confirm_cb(bool confirmed);
/* 系统升级处理 */
static void handle_system_upgrade(void);
/* 密码修改回调 */
static void password_change_cb(const char *old_pwd, const char *new_pwd);
static void handle_change_password(void);
static void update_nav_highlight(void);
static void time_update_timer_cb(lv_timer_t * timer);
extern void endoscope_main_init(void);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void endoscope_settings_init(void)
{
    /* 根据当前语言代码设置 current_lang 索引 */
    const char * current_lang_code = lang_get_current();
    if(strcmp(current_lang_code, "zh_CN") == 0) {
        current_lang = 0;
    } else if(strcmp(current_lang_code, "en_US") == 0) {
        current_lang = 1;
    }

    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, SETTING_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, 0);

    create_top_bar();
    create_nav_menu();
    create_content_area();
}

void endoscope_settings_show(void)
{
    current_page = 0;
    update_nav_highlight();
    lv_scr_load(settings_screen);
}

void endoscope_settings_hide(void)
{
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void create_top_bar(void)
{
    lv_obj_t * bar = lv_obj_create(settings_screen);
    lv_obj_set_size(bar, 1920, TOP_BAR_HEIGHT);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, SETTING_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    
    /* 设置图标和标题 */
    lv_obj_t * icon = lv_label_create(bar);
    lv_label_set_text(icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, SETTING_COLOR_TEXT, 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 36, 0);
    
    lv_obj_t * title = lv_label_create(bar);
    lv_label_set_text(title, _TR("SET_TITLE"));
    UI_SET_FONT(title);
    lv_obj_set_style_text_color(title, SETTING_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 96, 0);

    /* 返回按钮 */
    lv_obj_t * back_btn = lv_btn_create(bar);
    lv_obj_set_size(back_btn, 144, 72);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -36, 0);
    lv_obj_set_style_bg_color(back_btn, SETTING_COLOR_GRAY, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * back_lbl = lv_label_create(back_btn);
    lv_label_set_text_fmt(back_lbl, "%s " LV_SYMBOL_RIGHT, _TR("DLG_BACK"));
    UI_SET_FONT(back_lbl);
    lv_obj_set_style_text_color(back_lbl, SETTING_COLOR_TEXT, 0);
    lv_obj_center(back_lbl);
}

static void create_nav_menu(void)
{
    /* 左侧导航菜单背景 */
    nav_menu = lv_obj_create(settings_screen);
    lv_obj_set_size(nav_menu, LEFT_MENU_WIDTH, 960);
    lv_obj_align(nav_menu, LV_ALIGN_LEFT_MID, 0, TOP_BAR_HEIGHT/2);
    lv_obj_set_style_bg_color(nav_menu, SETTING_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(nav_menu, 0, 0);
    lv_obj_set_style_radius(nav_menu, 0, 0);
    lv_obj_set_style_pad_all(nav_menu, 0, 0);
    lv_obj_clear_flag(nav_menu, LV_OBJ_FLAG_SCROLLABLE);
    
    const char * nav_keys[4] = {"SET_NAV_KEYS", "SET_NAV_SYSTEM", "SET_NAV_ABOUT", "SET_NAV_POWER"};
    
    for(int i = 0; i < 4; i++) {
        nav_btns[i] = lv_btn_create(nav_menu);
        lv_obj_set_size(nav_btns[i], LEFT_MENU_WIDTH - 24, NAV_ITEM_HEIGHT);
        lv_obj_align(nav_btns[i], LV_ALIGN_TOP_MID, 0, 24 + i * (NAV_ITEM_HEIGHT + 12));
        lv_obj_set_style_bg_color(nav_btns[i], SETTING_COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(nav_btns[i], SETTING_COLOR_SELECTED, LV_STATE_CHECKED);
        lv_obj_set_style_radius(nav_btns[i], 8, 0);
        lv_obj_add_event_cb(nav_btns[i], nav_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        
        lv_obj_t * lbl = lv_label_create(nav_btns[i]);
        lv_label_set_text(lbl, _TR(nav_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, SETTING_COLOR_TEXT, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 48, 0);
        
        /* 右侧选中指示条 */
        lv_obj_t * indicator = lv_obj_create(nav_btns[i]);
        lv_obj_set_size(indicator, 6, NAV_ITEM_HEIGHT - 24);
        lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, -6, 0);
        lv_obj_set_style_bg_color(indicator, SETTING_COLOR_ACCENT, 0);
        lv_obj_set_style_radius(indicator, 3, 0);
        lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_content_area(void)
{
    content_area = lv_obj_create(settings_screen);
    lv_obj_set_size(content_area, 1920 - LEFT_MENU_WIDTH - 48, 960);
    lv_obj_align(content_area, LV_ALIGN_RIGHT_MID, -24, TOP_BAR_HEIGHT/2);
    lv_obj_set_style_bg_color(content_area, SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(content_area, 0, 0);
    lv_obj_set_style_radius(content_area, 16, 0);
    lv_obj_clear_flag(content_area, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 创建四个页面 */
    create_power_page();
    create_keys_page();
    create_system_page();
    create_about_page();
}

static void create_power_page(void)
{
    content_pages[3] = lv_obj_create(content_area);
    lv_obj_set_size(content_pages[3], lv_pct(100), lv_pct(100));
    lv_obj_align(content_pages[3], LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(content_pages[3], SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(content_pages[3], 0, 0);
    lv_obj_set_style_radius(content_pages[3], 0, 0);
    lv_obj_clear_flag(content_pages[3], LV_OBJ_FLAG_SCROLLABLE);
    
    /* 关机和重启按钮 */
    const char * btn_keys[2] = {"POWER_SHUTDOWN", "POWER_REBOOT"};
    const char * btn_icons[2] = {LV_SYMBOL_POWER, LV_SYMBOL_REFRESH};
    
    for(int i = 0; i < 2; i++) {
        lv_obj_t * btn = lv_btn_create(content_pages[3]);
        lv_obj_set_size(btn, 240, 240);
        lv_obj_align(btn, LV_ALIGN_CENTER, i == 0 ? -180 : 180, -60);
        lv_obj_set_style_bg_color(btn, SETTING_COLOR_SURFACE, 0);
        lv_obj_set_style_radius(btn, 120, 0);  /* 圆形 */
        lv_obj_set_style_border_width(btn, 4, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x666666), 0);
        lv_obj_add_event_cb(btn, power_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        
        /* 图标 */
        lv_obj_t * icon = lv_label_create(btn);
        lv_label_set_text(icon, btn_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(icon, SETTING_COLOR_ACCENT, 0);
        lv_obj_center(icon);
        
        /* 文字标签 */
        lv_obj_t * lbl = lv_label_create(content_pages[3]);
        lv_label_set_text(lbl, _TR(btn_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, SETTING_COLOR_TEXT, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, i == 0 ? -180 : 180, 150);
    }
}

static void create_keys_page(void)
{
    content_pages[0] = lv_obj_create(content_area);
    lv_obj_set_size(content_pages[0], lv_pct(100), lv_pct(100));
    lv_obj_align(content_pages[0], LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(content_pages[0], SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(content_pages[0], 0, 0);
    lv_obj_set_style_radius(content_pages[0], 0, 0);
    lv_obj_set_flex_flow(content_pages[0], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_pages[0], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_pages[0], 36, 0);
    lv_obj_set_style_pad_all(content_pages[0], 48, 0);
    
    /* 4行按键配置 */
    for(int row = 0; row < 4; row++) {
        lv_obj_t * row_cont = lv_obj_create(content_pages[0]);
        lv_obj_set_size(row_cont, lv_pct(100), 96);
        lv_obj_set_style_bg_color(row_cont, SETTING_COLOR_BG, 0);
        lv_obj_set_style_border_width(row_cont, 0, 0);
        lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row_cont, 24, 0);

        /* 状态圆点 */
        lv_obj_t * dot = lv_obj_create(row_cont);
        lv_obj_set_size(dot, 24, 24);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x666666), 0);
        lv_obj_set_style_radius(dot, 12, 0);
        lv_obj_set_style_border_width(dot, 0, 0);

        /* 按键标签 */
        lv_obj_t * lbl = lv_label_create(row_cont);
        lv_label_set_text_fmt(lbl, "%s%d:", _TR("KEY_PREFIX"), row + 1);
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, SETTING_COLOR_TEXT, 0);
        lv_obj_set_width(lbl, 120);

        /* 3个功能按钮 */
        const char * func_keys[4][3] = {
            {"KEY_BALANCE", "KEY_CAPTURE", "KEY_RECORD"},
            {"KEY_FREEZE", "KEY_CAPTURE", "KEY_ENHANCE"},
            {"KEY_DENOISE", "KEY_DEFOG", "KEY_GAMMA"},
            {"KEY_BALANCE", "KEY_CAPTURE", "KEY_RECORD"}
        };

        for(int j = 0; j < 3; j++) {
            lv_obj_t * btn = lv_btn_create(row_cont);
            lv_obj_set_size(btn, 192, 72);
            lv_obj_set_style_bg_color(btn, j == 0 ? SETTING_COLOR_ACCENT : SETTING_COLOR_GRAY, 0);
            lv_obj_set_style_radius(btn, 36, 0);
            lv_obj_add_event_cb(btn, key_config_btn_event_cb, LV_EVENT_CLICKED,
                              (void *)(intptr_t)(row * 3 + j));

            /* 保存按钮引用 */
            key_row_btns[row][j] = btn;

            lv_obj_t * btn_lbl = lv_label_create(btn);
            lv_label_set_text(btn_lbl, _TR(func_keys[row][j]));
            UI_SET_FONT(btn_lbl);
            lv_obj_set_style_text_color(btn_lbl, SETTING_COLOR_TEXT, 0);
            lv_obj_center(btn_lbl);
        }
    }
}

static void create_system_page(void)
{
    content_pages[1] = lv_obj_create(content_area);
    lv_obj_set_size(content_pages[1], lv_pct(100), lv_pct(100));
    lv_obj_align(content_pages[1], LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(content_pages[1], SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(content_pages[1], 0, 0);
    lv_obj_set_style_radius(content_pages[1], 0, 0);
    lv_obj_set_flex_flow(content_pages[1], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_pages[1], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_pages[1], 48, 0);
    lv_obj_set_style_pad_all(content_pages[1], 48, 0);
    
    /* 时间设置 */
    lv_obj_t * time_cont = lv_obj_create(content_pages[1]);
    lv_obj_set_size(time_cont, lv_pct(100), 96);
    lv_obj_set_style_bg_color(time_cont, SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(time_cont, 0, 0);
    lv_obj_set_flex_flow(time_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(time_cont, 36, 0);

    lv_obj_t * time_lbl = lv_label_create(time_cont);
    lv_label_set_text(time_lbl, _TR("SYS_TIME"));
    UI_SET_FONT(time_lbl);
    lv_obj_set_style_text_color(time_lbl, SETTING_COLOR_TEXT, 0);
    lv_obj_set_width(time_lbl, 120);

    /* 时间输入框 */
    time_input = lv_textarea_create(time_cont);
    lv_obj_set_size(time_input, 720, 72);

    ui_update_textarea_datetime(time_input);

    lv_obj_set_style_bg_color(time_input, SETTING_COLOR_SURFACE, 0);
    lv_obj_set_style_text_color(time_input, SETTING_COLOR_TEXT, 0);
    UI_SET_FONT(time_input);
    lv_obj_set_style_radius(time_input, 12, 0);
    lv_obj_set_style_pad_hor(time_input, 24, 0);
    lv_textarea_set_align(time_input, LV_TEXT_ALIGN_CENTER);

    /* 获取内部标签并设置垂直居中 */
    lv_obj_t * time_input_lbl = lv_textarea_get_label(time_input);
    lv_obj_align(time_input_lbl, LV_ALIGN_CENTER, 0, 0);

    /* 创建定时器每秒更新时间 */
    time_timer = lv_timer_create(time_update_timer_cb, 1000, NULL);

    /* 语言选择 */
    lv_obj_t * lang_cont = lv_obj_create(content_pages[1]);
    lv_obj_set_size(lang_cont, lv_pct(100), 96);
    lv_obj_set_style_bg_color(lang_cont, SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(lang_cont, 0, 0);
    lv_obj_set_flex_flow(lang_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lang_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lang_cont, 36, 0);

    lv_obj_t * lang_lbl = lv_label_create(lang_cont);
    lv_label_set_text(lang_lbl, _TR("SYS_LANGUAGE"));
    UI_SET_FONT(lang_lbl);
    lv_obj_set_style_text_color(lang_lbl, SETTING_COLOR_TEXT, 0);
    lv_obj_set_width(lang_lbl, 120);

    /* 语言切换按钮组 */
    lv_obj_t * lang_btn_cont = lv_obj_create(lang_cont);
    lv_obj_set_size(lang_btn_cont, 480, 72);
    lv_obj_set_style_bg_color(lang_btn_cont, SETTING_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(lang_btn_cont, 12, 0);
    lv_obj_set_style_border_width(lang_btn_cont, 0, 0);
    lv_obj_set_flex_flow(lang_btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(lang_btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(lang_btn_cont, 0, 0);
    lv_obj_clear_flag(lang_btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    const char * lang_keys[2] = {"LANG_CHINESE", "LANG_ENGLISH"};
    for(int i = 0; i < 2; i++) {
        lv_obj_t * btn = lv_btn_create(lang_btn_cont);
        lv_obj_set_size(btn, 228, 60);
        lv_obj_set_style_bg_color(btn, i == current_lang ? SETTING_COLOR_ACCENT : SETTING_COLOR_SURFACE, 0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_add_event_cb(btn, lang_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* 保存按钮引用 */
        lang_btns[i] = btn;

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, _TR(lang_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, SETTING_COLOR_TEXT, 0);
        lv_obj_center(lbl);
    }

    /* 底部按钮组 */
    lv_obj_t * btn_cont = lv_obj_create(content_pages[1]);
    lv_obj_set_size(btn_cont, lv_pct(100), 96);
    lv_obj_set_style_bg_color(btn_cont, SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_cont, 36, 0);

    const char * sys_btn_keys[4] = {"SYS_UPGRADE", "SYS_FACTORY_RESET",
                                     "SYS_CHANGE_PASSWORD", "FPN 校准"};
    for(int i = 0; i < 4; i++) {
        lv_obj_t * btn = lv_btn_create(btn_cont);
        lv_obj_set_size(btn, 288, 84);
        lv_obj_set_style_bg_color(btn, SETTING_COLOR_GRAY, 0);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_add_event_cb(btn, system_btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, _TR(sys_btn_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, SETTING_COLOR_TEXT, 0);
        lv_obj_center(lbl);
    }
}

static void create_about_page(void)
{
    content_pages[2] = lv_obj_create(content_area);
    lv_obj_set_size(content_pages[2], lv_pct(100), lv_pct(100));
    lv_obj_align(content_pages[2], LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(content_pages[2], SETTING_COLOR_BG, 0);
    lv_obj_set_style_border_width(content_pages[2], 0, 0);
    lv_obj_set_style_radius(content_pages[2], 0, 0);
    lv_obj_set_flex_flow(content_pages[2], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_pages[2], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_pages[2], 36, 0);
    lv_obj_set_style_pad_all(content_pages[2], 48, 0);

    /* 信息项 */
    const char * label_keys[3] = {"ABOUT_COMPANY", "ABOUT_DEVICE", "ABOUT_MODEL"};
    const char * values[3] = {
        "",
        _TR("DEVICE_NAME"),
        "XXXXXXXXXXXX"
    };

    for(int i = 0; i < 3; i++) {
        lv_obj_t * info_cont = lv_obj_create(content_pages[2]);
        lv_obj_set_size(info_cont, lv_pct(100), 84);
        lv_obj_set_style_bg_color(info_cont, SETTING_COLOR_SURFACE, 0);
        lv_obj_set_style_radius(info_cont, 12, 0);
        lv_obj_set_style_border_width(info_cont, 0, 0);
        lv_obj_set_flex_flow(info_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(info_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(info_cont, 24, 0);
        lv_obj_set_style_pad_left(info_cont, 36, 0);

        lv_obj_t * lbl = lv_label_create(info_cont);
        lv_label_set_text(lbl, _TR(label_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, SETTING_COLOR_TEXT, 0);

        if(strlen(values[i]) > 0) {
            lv_obj_t * val = lv_label_create(info_cont);
            lv_label_set_text(val, values[i]);
            UI_SET_FONT(val);
            lv_obj_set_style_text_color(val, SETTING_COLOR_TEXT, 0);
        }
    }
}

static void update_nav_highlight(void)
{
    for(int i = 0; i < 4; i++) {
        if(i == current_page) {
            lv_obj_add_state(nav_btns[i], LV_STATE_CHECKED);
            /* 显示选中指示条 */
            lv_obj_t * indicator = lv_obj_get_child(nav_btns[i], 1);
            if(indicator) lv_obj_clear_flag(indicator, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_state(nav_btns[i], LV_STATE_CHECKED);
            /* 隐藏选中指示条 */
            lv_obj_t * indicator = lv_obj_get_child(nav_btns[i], 1);
            if(indicator) lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
        }
        
        /* 显示/隐藏对应页面 */
        if(content_pages[i]) {
            if(i == current_page) {
                lv_obj_clear_flag(content_pages[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(content_pages[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void nav_btn_event_cb(lv_event_t * e)
{
    int page_id = (int)(intptr_t)lv_event_get_user_data(e);
    current_page = page_id;
    update_nav_highlight();
}

static void back_btn_event_cb(lv_event_t * e)
{
    (void)e;
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
}

static void power_btn_event_cb(lv_event_t * e)
{
    int btn_id = (int)(intptr_t)lv_event_get_user_data(e);
    if(btn_id == 0) {
        /* 关机 */
        LV_LOG_USER("Shutdown requested");
    } else {
        /* 重启 */
        LV_LOG_USER("Reboot requested");
    }
}

static void key_config_btn_event_cb(lv_event_t * e)
{
    int btn_id = (int)(intptr_t)lv_event_get_user_data(e);
    int row = btn_id / 3;
    int col = btn_id % 3;
    
    /* 如果点击的是当前已选中的按钮，不做任何操作 */
    if(key_row_selected[row] == col) {
        return;
    }
    
    /* 将之前选中的按钮设为灰色 */
    int prev_selected = key_row_selected[row];
    if(key_row_btns[row][prev_selected]) {
        lv_obj_set_style_bg_color(key_row_btns[row][prev_selected], SETTING_COLOR_GRAY, 0);
    }
    
    /* 将新选中的按钮设为蓝色 */
    lv_obj_set_style_bg_color(key_row_btns[row][col], SETTING_COLOR_ACCENT, 0);
    
    /* 更新选中状态 */
    key_row_selected[row] = col;
    
    LV_LOG_USER("Key row %d selected: %d", row, col);
}

static void system_btn_event_cb(lv_event_t * e)
{
    int btn_id = (int)(intptr_t)lv_event_get_user_data(e);
    printf("[FPN-UI] system_btn_event: btn_id=%d\n", btn_id);
    
    switch(btn_id) {
    case 0: /* 系统升级 */
        handle_system_upgrade();
        break;
    case 1: /* 恢复出厂设置 */
        endoscope_dialogs_confirm(
            _TR("DLG_FACTORY_RESET_TITLE"),
            _TR("DLG_FACTORY_RESET_MSG"),
            _TR("DLG_BTN_YES"),
            _TR("DLG_BTN_NO"),
            factory_reset_confirm_cb
        );
        break;
    case 2: /* 修改密码 */
        handle_change_password();
        break;
    case 3: /* FPN 校准 */
        printf("[FPN-UI] showing confirm dialog...\n");
        endoscope_dialogs_confirm(
            "FPN 校准",
            "请确保镜体已完全遮光\n然后点击确定开始校准",
            _TR("DLG_BTN_OK"), _TR("DLG_CANCEL"),
            do_fpn_confirm_cb);
        break;
    }
}

static void password_change_cb(const char *old_pwd, const char *new_pwd)
{
    (void)old_pwd;
    
    /* 更新密码 */
    if(endoscope_dialogs_set_password(new_pwd)) {
        LV_LOG_USER("Password changed successfully");
        /* 显示成功提示 */
        endoscope_dialogs_success(
            _TR("DLG_CHANGE_PASSWORD_TITLE"),
            "Password updated successfully",
            _TR("DLG_BTN_OK")
        );
    }
}

static void handle_change_password(void)
{
    endoscope_dialogs_password_change(password_change_cb);
}

static void do_fpn_confirm_cb(bool confirmed)
{
    printf("[FPN-UI] confirm cb: confirmed=%d, starting calibration\n", confirmed);
    fflush(stdout);
    if (confirmed) do_fpn_calibration();
}

static void do_fpn_calibration(void)
{
    video_context_t *vc = video_get_context();
    if (!vc) { printf("[FPN] no video context\n"); return; }

    /* Ctrl-C 重启后 VI 状态不支持切模式, 提示冷启动 */
    if (mpp_fpn_get_status() == FPN_STATUS_IDLE && vc->state == VIDEO_STATE_INIT) {
        printf("[FPN] MPP reused session, calibration may fail, try anyway...\n");
    }

    printf("[FPN] stopping preview for calibration...\n");
    vc->b_running = HI_FALSE;
    video_stop(vc);
    SAMPLE_COMM_VPSS_UnBind_VO(vc->vpss_grp, vc->vpss_chn, vc->vo_dev, vc->vo_chn);
    SAMPLE_COMM_VI_UnBind_VPSS(vc->vi_pipe, vc->vi_chn, vc->vpss_grp);
    { HI_BOOL ab[VPSS_MAX_PHY_CHN_NUM] = {0}; ab[vc->vpss_chn] = HI_TRUE;
      SAMPLE_COMM_VPSS_Stop(vc->vpss_grp, ab); }

    fpn_status_t ret = mpp_fpn_calibrate(vc->vi_pipe);
    printf("[FPN] calibration returned %d, rebuilding VPSS...\n", ret);

    { VPSS_GRP_ATTR_S g; VPSS_CHN_ATTR_S ac[VPSS_MAX_PHY_CHN_NUM];
      HI_BOOL ab[VPSS_MAX_PHY_CHN_NUM] = {0};
      memset(&g, 0, sizeof(g));
      g.stFrameRate.s32SrcFrameRate = -1; g.stFrameRate.s32DstFrameRate = -1;
      g.enDynamicRange = DYNAMIC_RANGE_SDR8;
      g.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
      g.u32MaxW = vc->sensor->width; g.u32MaxH = vc->sensor->height;
      g.bNrEn = HI_FALSE; /* 3DNR关闭 */
      g.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
      g.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
      memset(ac, 0, sizeof(ac));
      ac[vc->vpss_chn].u32Width = vc->sensor->width;
      ac[vc->vpss_chn].u32Height = vc->sensor->height;
      ac[vc->vpss_chn].enChnMode = VPSS_CHN_MODE_USER;
      ac[vc->vpss_chn].enCompressMode = COMPRESS_MODE_NONE;
      ac[vc->vpss_chn].enDynamicRange = DYNAMIC_RANGE_SDR8;
      ac[vc->vpss_chn].enVideoFormat = VIDEO_FORMAT_LINEAR;
      ac[vc->vpss_chn].enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
      ac[vc->vpss_chn].stFrameRate.s32SrcFrameRate = (HI_S32)vc->sensor->fps;
      ac[vc->vpss_chn].stFrameRate.s32DstFrameRate = (HI_S32)vc->sensor->fps;
      ab[vc->vpss_chn] = HI_TRUE;
      printf("[FPN] starting VPSS...\n");
      HI_S32 r = SAMPLE_COMM_VPSS_Start(vc->vpss_grp, ab, &g, ac);
      printf("[FPN] VPSS start ret=0x%x\n", r); }
    printf("[FPN] binding VI->VPSS...\n");
    SAMPLE_COMM_VI_Bind_VPSS(vc->vi_pipe, vc->vi_chn, vc->vpss_grp);
    printf("[FPN] binding VPSS->VO...\n");
    SAMPLE_COMM_VPSS_Bind_VO(vc->vpss_grp, vc->vpss_chn, vc->vo_dev, vc->vo_chn);
    printf("[FPN] starting video...\n");
    video_start(vc);
    printf("[FPN] video restarted\n");

    if (ret == FPN_STATUS_OK)
        endoscope_dialogs_success("FPN", "校准成功!", _TR("DLG_BTN_OK"));
    else if (ret == FPN_STATUS_FAILED)
        endoscope_dialogs_success("FPN",
            "校准失败\nCtrl-C重启后需冷启动再校准",
            _TR("DLG_BTN_OK"));
    else
        endoscope_dialogs_success("FPN", "校准失败, 请遮光后重试", _TR("DLG_BTN_OK"));
}

static void factory_reset_confirm_cb(bool confirmed)
{
    if(confirmed) {
        LV_LOG_USER("Factory reset confirmed - resetting settings...");
        /* TODO: 实际执行恢复出厂设置操作 */
        /* 可以显示一个进度对话框 */
    } else {
        LV_LOG_USER("Factory reset cancelled");
    }
}

static void handle_system_upgrade(void)
{
    /* TODO: 实际执行系统升级操作 */
    LV_LOG_USER("System upgrade starting...");
    
    /* 显示升级成功对话框 */
    endoscope_dialogs_success(
        _TR("DLG_UPGRADE_TITLE"),
        _TR("DLG_UPGRADE_SUCCESS_MSG"),
        _TR("DLG_BTN_OK")
    );
}

static void lang_btn_event_cb(lv_event_t * e)
{
    int lang_id = (int)(intptr_t)lv_event_get_user_data(e);

    /* 如果点击的是当前已选中的语言，不做任何操作 */
    if(current_lang == lang_id) {
        return;
    }

    /* 切换语言 */
    const char * lang_codes[2] = {"zh_CN", "en_US"};
    const char * lang_names[2] = {"LANG_CHINESE", "LANG_ENGLISH"};

    LV_LOG_USER("Switching language to: %s", lang_id == 0 ? "Chinese" : "English");

    /* 加载新语言和字体 - 先加载，后续创建页面时使用新语言 */
    lang_load(lang_codes[lang_id]);
    font_manager_load_for_language(lang_codes[lang_id]);

    /* 更新当前语言 */
    current_lang = lang_id;

    /* 重建主界面以应用新语言 */
    endoscope_main_init();
    screen_manager_invalidate(ENDOSCOPE_SCREEN_MAIN);

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);

    if(time_timer) {
        lv_timer_delete(time_timer);
        time_timer = NULL;
    }

    if(settings_screen) {
        lv_obj_del(settings_screen);
        settings_screen = NULL;
    }

    /* 清除所有控件引用 */
    nav_menu = NULL;
    content_area = NULL;
    for(int i = 0; i < 4; i++) {
        nav_btns[i] = NULL;
        content_pages[i] = NULL;
    }
    for(int i = 0; i < 2; i++) {
        lang_btns[i] = NULL;
    }
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 3; j++) {
            key_row_btns[i][j] = NULL;
        }
    }

    /* 重新初始化设置页面 */
    endoscope_settings_init();
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_SETTINGS);

    /* 切换到系统页面 (当前所在页面) */
    current_page = 1;
    update_nav_highlight();

    LV_LOG_USER("Language switched to: %s, page recreated", lang_get_text(lang_names[lang_id]));

    /* 保存语言配置，下次开机自动加载 */
    lang_save_config(lang_codes[lang_id]);
}

/**
 * @brief 时间更新定时器回调 - 每秒更新一次时间显示
 */
static void time_update_timer_cb(lv_timer_t * timer)
{
    (void)timer;

    if(time_input == NULL) return;

    ui_update_textarea_datetime(time_input);
}
