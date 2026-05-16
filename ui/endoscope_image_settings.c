/**
 * @file endoscope_image_settings.c
 * @brief 图像设置实现
 */

#include "endoscope_image_settings.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "lang_manager.h"
#include "ui_theme.h"

static lv_obj_t * image_settings_screen = NULL;

static void back_btn_event(lv_event_t * e);
static void slider_event(lv_event_t * e);

void endoscope_image_settings_init(void)
{
    image_settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(image_settings_screen, UI_COLOR_BG, 0);

    /* Header */
    lv_obj_t * header = lv_obj_create(image_settings_screen);
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
    lv_label_set_text(back_lbl, _TR("IMG_SET_BACK"));
    lv_obj_center(back_lbl);

    /* Title */
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, _TR("IMG_SET_TITLE"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_center(title);

    /* 设置项容器 */
    lv_obj_t * cont = lv_obj_create(image_settings_screen);
    lv_obj_set_size(cont, 760, 380);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(cont, 20, 0);
    lv_obj_set_style_pad_bottom(cont, 20, 0);
    lv_obj_set_style_pad_gap(cont, 20, 0);

    /* 亮度调节 */
    lv_obj_t * row1 = lv_obj_create(cont);
    lv_obj_set_size(row1, 700, 50);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);

    lv_obj_t * lbl1 = lv_label_create(row1);
    lv_label_set_text(lbl1, _TR("IMG_BRIGHTNESS"));
    lv_obj_align(lbl1, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * slider1 = lv_slider_create(row1);
    lv_obj_set_size(slider1, 500, 20);
    lv_obj_align(slider1, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_slider_set_range(slider1, 0, 100);
    lv_slider_set_value(slider1, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider1, slider_event, LV_EVENT_VALUE_CHANGED, NULL);

    /* Contrast */
    lv_obj_t * row2 = lv_obj_create(cont);
    lv_obj_set_size(row2, 700, 50);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);

    lv_obj_t * lbl2 = lv_label_create(row2);
    lv_label_set_text(lbl2, _TR("IMG_CONTRAST"));
    lv_obj_align(lbl2, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * slider2 = lv_slider_create(row2);
    lv_obj_set_size(slider2, 500, 20);
    lv_obj_align(slider2, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_slider_set_range(slider2, 0, 100);
    lv_slider_set_value(slider2, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider2, slider_event, LV_EVENT_VALUE_CHANGED, NULL);

    /* Saturation */
    lv_obj_t * row3 = lv_obj_create(cont);
    lv_obj_set_size(row3, 700, 50);
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);

    lv_obj_t * lbl3 = lv_label_create(row3);
    lv_label_set_text(lbl3, _TR("IMG_SATURATION"));
    lv_obj_align(lbl3, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * slider3 = lv_slider_create(row3);
    lv_obj_set_size(slider3, 500, 20);
    lv_obj_align(slider3, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_slider_set_range(slider3, 0, 100);
    lv_slider_set_value(slider3, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider3, slider_event, LV_EVENT_VALUE_CHANGED, NULL);

    /* Sharpness */
    lv_obj_t * row4 = lv_obj_create(cont);
    lv_obj_set_size(row4, 700, 50);
    lv_obj_set_style_bg_opa(row4, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row4, 0, 0);

    lv_obj_t * lbl4 = lv_label_create(row4);
    lv_label_set_text(lbl4, _TR("IMG_SHARPNESS"));
    lv_obj_align(lbl4, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * slider4 = lv_slider_create(row4);
    lv_obj_set_size(slider4, 500, 20);
    lv_obj_align(slider4, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_slider_set_range(slider4, 0, 100);
    lv_slider_set_value(slider4, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider4, slider_event, LV_EVENT_VALUE_CHANGED, NULL);
}

void endoscope_image_settings_show(void)
{
    lv_scr_load(image_settings_screen);
}

void endoscope_image_settings_hide(void)
{
}

static void back_btn_event(lv_event_t * e)
{
    (void)e;
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
}

static void slider_event(lv_event_t * e)
{
    (void)e;
}
