/**
 * @file endoscope_splash.c
 * @brief 开机画面实现
 */

/*********************
 *      INCLUDES
 *********************/
#include "endoscope_splash.h"
#include "endoscope_ui.h"
#include "lang_manager.h"
#include "font_manager.h"
#include "ui_helpers.h"

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * splash_screen = NULL;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void endoscope_splash_init(void)
{
    /* 创建开机画面 - 深蓝背景 */
    splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x0a1628), 0);
    lv_obj_set_style_bg_opa(splash_screen, LV_OPA_COVER, 0);

    /* 主标题 - 居中显示，使用更大的TTF字体 */
    lv_obj_t * title = lv_label_create(splash_screen);
    lv_label_set_text(title, _TR("SPLASH_TITLE"));
    /* 使用大字体 */
    lv_font_t * large_font = font_manager_get_font_large();
    if(large_font) {
        lv_obj_set_style_text_font(title, large_font, 0);
    } else {
        UI_SET_FONT(title);
    }
    /* 设置更大的字体大小样式 */
    lv_obj_set_style_text_letter_space(title, 6, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_center(title);
}

extern volatile int g_splash_showing;

void endoscope_splash_show(void)
{
    g_splash_showing = 1;
    lv_scr_load(splash_screen);
}

void endoscope_splash_hide(void)
{
    g_splash_showing = 0;
}
