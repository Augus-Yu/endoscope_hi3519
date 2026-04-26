/**
 * @file endoscope_ui.c
 * @brief 医疗电子内窥镜UI主实现
 */

/*********************
 *      INCLUDES
 *********************/
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "endoscope_splash.h"
#include "endoscope_main.h"
#include "endoscope_settings.h"
#include "endoscope_playback.h"
#include "endoscope_image_settings.h"
#include "endoscope_dialogs.h"
#include "lang_manager.h"
#include "font_manager.h"
#include "ui_helpers.h"
#include "src/widgets/keyboard/lv_keyboard.h"
#include "src/others/ime/lv_ime_pinyin.h"
#include <string.h>

static void splash_timer_cb(lv_timer_t * timer);
extern volatile int g_dialog_showing;

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static endoscope_status_t status = {
    .endoscope_connected = false,
    .usb_connected = false,
    .is_recording = false,
    .recording_time = 0,
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void endoscope_ui_init(void)
{
    /* 初始化字体管理器 (TTF 实时渲染) */
    if (!font_manager_init("./lang/fonts")) {
        LV_LOG_ERROR("Font manager init failed");
    }
    
    /* 初始化多语言系统 - 会自动读取上次保存的语言配置 */
    if (!lang_manager_init("./lang")) {
        LV_LOG_ERROR("Lang manager init failed");
    }
    
    /* 根据当前语言加载对应字体 */
    if (!font_manager_load_for_language(lang_get_current())) {
        LV_LOG_ERROR("Failed to load font for language: %s", lang_get_current());
    }

    /* 设置主题 - 使用当前字体 */
    lv_theme_t * theme = lv_theme_default_init(
        lv_display_get_default(),
        UI_COLOR_PRIMARY,
        UI_COLOR_SECONDARY,
        true,
        font_manager_get_font()
    );
    lv_display_set_theme(lv_display_get_default(), theme);

    screen_manager_init();
    screen_manager_register(ENDOSCOPE_SCREEN_SPLASH,
                            endoscope_splash_init,
                            endoscope_splash_show,
                            endoscope_splash_hide);
    screen_manager_register(ENDOSCOPE_SCREEN_MAIN,
                            endoscope_main_init,
                            endoscope_main_show,
                            endoscope_main_hide);
    screen_manager_register(ENDOSCOPE_SCREEN_SETTINGS,
                            endoscope_settings_init,
                            endoscope_settings_show,
                            endoscope_settings_hide);
    screen_manager_register(ENDOSCOPE_SCREEN_PLAYBACK,
                            endoscope_playback_init,
                            endoscope_playback_show,
                            endoscope_playback_hide);
    screen_manager_register(ENDOSCOPE_SCREEN_IMAGE_SETTINGS,
                            endoscope_image_settings_init,
                            endoscope_image_settings_show,
                            endoscope_image_settings_hide);

    endoscope_dialogs_init();

    /* 显示开机画面 */
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_SPLASH);
    lv_timer_create(splash_timer_cb, 3000, NULL);
}

static void splash_timer_cb(lv_timer_t * timer)
{
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
    lv_timer_delete(timer);
}

endoscope_screen_t endoscope_get_current_screen(void)
{
    return screen_manager_get_current();
}

endoscope_status_t * endoscope_get_status(void)
{
    return &status;
}

void endoscope_set_endoscope_connected(bool connected)
{
    status.endoscope_connected = connected;
    if(!connected && screen_manager_get_current() == ENDOSCOPE_SCREEN_MAIN) {
        endoscope_show_dialog("Notice", "Endoscope not connected", "OK");
    }
}

void endoscope_set_usb_connected(bool connected)
{
    status.usb_connected = connected;
}

void endoscope_show_dialog(const char *title, const char *msg, const char *btn_text)
{
    endoscope_dialogs_show(title, msg, btn_text);
}

void endoscope_hide_dialog(void)
{
    endoscope_dialogs_hide();
}

/**********************
 *   PATIENT INFO FUNCTIONS
 **********************/

static lv_obj_t * patient_overlay = NULL;
static lv_obj_t * patient_dialog = NULL;
static lv_obj_t * txt_id = NULL;
static lv_obj_t * txt_name = NULL;
static lv_obj_t * dd_gender = NULL;
static lv_obj_t * txt_age = NULL;
static lv_obj_t * patient_kb_cn = NULL;
static lv_obj_t * patient_kb_en = NULL;
static lv_obj_t * patient_ime = NULL;
static lv_obj_t * patient_cand_panel = NULL;
static lv_obj_t * patient_kb_toggle_btn = NULL;
static lv_obj_t * patient_kb_current_ta = NULL;
static int patient_kb_lang = 0;

static void patient_dialog_event_cb(lv_event_t * e);
static void patient_kb_event_cb(lv_event_t * e);
static void patient_ta_event_cb(lv_event_t * e);
static void patient_kb_toggle_event_cb(lv_event_t * e);
static void create_patient_dialog(void);
static void show_patient_kb(lv_obj_t * ta, lv_keyboard_mode_t mode);
static void patient_kb_switch_language(void);

static void patient_kb_switch_language(void)
{
    patient_kb_lang = !patient_kb_lang;

    if(patient_kb_lang == 0) {
        if(patient_kb_en) lv_obj_add_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_cn) {
            lv_obj_remove_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
            if(patient_kb_current_ta) {
                lv_keyboard_set_textarea(patient_kb_cn, patient_kb_current_ta);
            }
        }
        if(patient_cand_panel) lv_obj_remove_flag(patient_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_toggle_btn) lv_label_set_text(lv_obj_get_child(patient_kb_toggle_btn, 0), "EN");
    } else {
        if(patient_kb_cn) lv_obj_add_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_en) {
            lv_obj_remove_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
            if(patient_kb_current_ta) {
                lv_keyboard_set_textarea(patient_kb_en, patient_kb_current_ta);
            }
        }
        if(patient_cand_panel) lv_obj_add_flag(patient_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_toggle_btn) lv_label_set_text(lv_obj_get_child(patient_kb_toggle_btn, 0), "中");
    }
}

static void show_patient_kb(lv_obj_t * ta, lv_keyboard_mode_t mode)
{
    patient_kb_current_ta = ta;

    if(mode == LV_KEYBOARD_MODE_NUMBER) {
        if(patient_kb_cn) lv_obj_add_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
        if(patient_cand_panel) lv_obj_add_flag(patient_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_toggle_btn) lv_obj_add_flag(patient_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_en) {
            lv_keyboard_set_mode(patient_kb_en, LV_KEYBOARD_MODE_NUMBER);
            lv_keyboard_set_textarea(patient_kb_en, ta);
            lv_obj_remove_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if(patient_kb_toggle_btn) lv_obj_remove_flag(patient_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);

    if(patient_kb_lang == 0) {
        if(patient_kb_en) lv_obj_add_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_cn) {
            lv_keyboard_set_mode(patient_kb_cn, mode);
            lv_keyboard_set_textarea(patient_kb_cn, ta);
            lv_obj_remove_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
        }
        if(patient_cand_panel) lv_obj_remove_flag(patient_cand_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        if(patient_kb_cn) lv_obj_add_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_en) {
            lv_keyboard_set_mode(patient_kb_en, mode);
            lv_keyboard_set_textarea(patient_kb_en, ta);
            lv_obj_remove_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
        }
        if(patient_cand_panel) lv_obj_add_flag(patient_cand_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void patient_ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target_obj(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_mode_t mode = (lv_keyboard_mode_t)(intptr_t)lv_event_get_user_data(e);
        show_patient_kb(ta, mode);
    }
}

static void patient_kb_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        if(patient_kb_cn) lv_obj_add_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_en) lv_obj_add_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
        if(patient_cand_panel) lv_obj_add_flag(patient_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(patient_kb_toggle_btn) lv_obj_add_flag(patient_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);
        lv_indev_reset(NULL, NULL);
    }
}

static void patient_kb_toggle_event_cb(lv_event_t * e)
{
    (void)e;
    patient_kb_switch_language();
}

static void create_patient_dialog(void)
{
    if(patient_overlay) return;

    g_dialog_showing = 1;

    patient_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(patient_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(patient_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(patient_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(patient_overlay, LV_OBJ_FLAG_CLICKABLE);

    patient_dialog = lv_obj_create(patient_overlay);
    lv_obj_set_size(patient_dialog, 800, 480);
    lv_obj_align(patient_dialog, LV_ALIGN_TOP_MID, 0, 200);
    lv_obj_set_style_bg_color(patient_dialog, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(patient_dialog, 16, 0);
    lv_obj_set_style_pad_all(patient_dialog, 24, 0);
    lv_obj_clear_flag(patient_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(patient_dialog);
    lv_label_set_text(title, _TR("DLG_TITLE_PATIENT"));
    UI_SET_FONT(title);
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * form = lv_obj_create(patient_dialog);
    lv_obj_set_size(form, 720, 280);
    lv_obj_align(form, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(form, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_style_pad_all(form, 24, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(form, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(form, 8, 0);
    lv_obj_clear_flag(form, LV_OBJ_FLAG_SCROLLABLE);

    patient_info_t * patient = endoscope_get_patient_info();
    const char * lang = lang_get_current();
    bool is_zh = (strncmp(lang, "zh", 2) == 0);

    lv_obj_t * row_id = lv_obj_create(form);
    lv_obj_set_size(row_id, 648, 52);
    lv_obj_set_style_bg_color(row_id, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(row_id, 0, 0);
    lv_obj_clear_flag(row_id, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_id, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_id, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_id, 24, 0);

    lv_obj_t * lbl_id = lv_label_create(row_id);
    lv_label_set_text(lbl_id, _TR("MAIN_PATIENT_ID"));
    UI_SET_FONT(lbl_id);
    lv_obj_set_style_text_color(lbl_id, UI_COLOR_TEXT, 0);
    lv_obj_set_width(lbl_id, 120);

    txt_id = lv_textarea_create(row_id);
    lv_obj_set_size(txt_id, 480, 44);
    lv_textarea_set_one_line(txt_id, true);
    lv_textarea_set_text(txt_id, patient->id);
    lv_obj_set_style_bg_color(txt_id, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_text_color(txt_id, UI_COLOR_TEXT, 0);
    UI_SET_FONT(txt_id);
    lv_obj_set_style_radius(txt_id, 8, 0);
    lv_obj_set_style_pad_hor(txt_id, 16, 0);
    lv_obj_add_event_cb(txt_id, patient_ta_event_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)LV_KEYBOARD_MODE_NUMBER);

    lv_obj_t * row_name = lv_obj_create(form);
    lv_obj_set_size(row_name, 648, 52);
    lv_obj_set_style_bg_color(row_name, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(row_name, 0, 0);
    lv_obj_clear_flag(row_name, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_name, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_name, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_name, 24, 0);

    lv_obj_t * lbl_name = lv_label_create(row_name);
    lv_label_set_text(lbl_name, _TR("MAIN_PATIENT_NAME"));
    UI_SET_FONT(lbl_name);
    lv_obj_set_style_text_color(lbl_name, UI_COLOR_TEXT, 0);
    lv_obj_set_width(lbl_name, 120);

    txt_name = lv_textarea_create(row_name);
    lv_obj_set_size(txt_name, 480, 44);
    lv_textarea_set_one_line(txt_name, true);
    lv_textarea_set_text(txt_name, patient->name);
    lv_obj_set_style_bg_color(txt_name, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_text_color(txt_name, UI_COLOR_TEXT, 0);
    UI_SET_FONT(txt_name);
    lv_obj_set_style_radius(txt_name, 8, 0);
    lv_obj_set_style_pad_hor(txt_name, 16, 0);
    lv_obj_add_event_cb(txt_name, patient_ta_event_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)LV_KEYBOARD_MODE_TEXT_LOWER);

    lv_obj_t * row_gender = lv_obj_create(form);
    lv_obj_set_size(row_gender, 648, 52);
    lv_obj_set_style_bg_color(row_gender, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(row_gender, 0, 0);
    lv_obj_clear_flag(row_gender, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_gender, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_gender, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_gender, 24, 0);

    lv_obj_t * lbl_gender = lv_label_create(row_gender);
    lv_label_set_text(lbl_gender, _TR("MAIN_PATIENT_GENDER"));
    UI_SET_FONT(lbl_gender);
    lv_obj_set_style_text_color(lbl_gender, UI_COLOR_TEXT, 0);
    lv_obj_set_width(lbl_gender, 120);

    dd_gender = lv_dropdown_create(row_gender);
    lv_obj_set_size(dd_gender, 480, 44);
    if(is_zh) {
        lv_dropdown_set_options(dd_gender, "男\n女");
    } else {
        lv_dropdown_set_options(dd_gender, "Male\nFemale");
    }
    lv_obj_set_style_bg_color(dd_gender, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_text_color(dd_gender, UI_COLOR_TEXT, 0);
    UI_SET_FONT(dd_gender);
    lv_obj_set_style_radius(dd_gender, 8, 0);
    lv_obj_set_style_pad_hor(dd_gender, 16, 0);
    if(patient->gender[0]) {
        if((is_zh && strcmp(patient->gender, "男") == 0) || (!is_zh && strcasecmp(patient->gender, "Male") == 0)) {
            lv_dropdown_set_selected(dd_gender, 0);
        } else {
            lv_dropdown_set_selected(dd_gender, 1);
        }
    }

    lv_obj_t * row_age = lv_obj_create(form);
    lv_obj_set_size(row_age, 648, 52);
    lv_obj_set_style_bg_color(row_age, UI_COLOR_BG, 0);
    lv_obj_set_style_border_width(row_age, 0, 0);
    lv_obj_clear_flag(row_age, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_age, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_age, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_age, 24, 0);

    lv_obj_t * lbl_age = lv_label_create(row_age);
    lv_label_set_text(lbl_age, _TR("MAIN_PATIENT_AGE"));
    UI_SET_FONT(lbl_age);
    lv_obj_set_style_text_color(lbl_age, UI_COLOR_TEXT, 0);
    lv_obj_set_width(lbl_age, 120);

    txt_age = lv_textarea_create(row_age);
    lv_obj_set_size(txt_age, 480, 44);
    lv_textarea_set_one_line(txt_age, true);
    lv_textarea_set_text(txt_age, patient->age);
    lv_obj_set_style_bg_color(txt_age, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_text_color(txt_age, UI_COLOR_TEXT, 0);
    UI_SET_FONT(txt_age);
    lv_obj_set_style_radius(txt_age, 8, 0);
    lv_obj_set_style_pad_hor(txt_age, 16, 0);
    lv_obj_add_event_cb(txt_age, patient_ta_event_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)LV_KEYBOARD_MODE_NUMBER);

    patient_ime = lv_ime_pinyin_create(patient_overlay);
    lv_obj_set_style_text_font(patient_ime, font_manager_get_font(), 0);

    patient_kb_en = lv_keyboard_create(patient_overlay);
    lv_obj_set_size(patient_kb_en, LV_HOR_RES, 280);
    lv_obj_align(patient_kb_en, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(patient_kb_en, NULL);
    lv_obj_add_flag(patient_kb_en, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(patient_kb_en, patient_kb_event_cb, LV_EVENT_ALL, NULL);

    patient_kb_cn = lv_keyboard_create(patient_overlay);
    lv_obj_set_size(patient_kb_cn, LV_HOR_RES, 280);
    lv_obj_align(patient_kb_cn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(patient_kb_cn, NULL);
    lv_obj_add_flag(patient_kb_cn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(patient_kb_cn, patient_kb_event_cb, LV_EVENT_ALL, NULL);
    lv_ime_pinyin_set_keyboard(patient_ime, patient_kb_cn);

    patient_cand_panel = lv_ime_pinyin_get_cand_panel(patient_ime);
    lv_obj_set_size(patient_cand_panel, LV_HOR_RES, 48);
    lv_obj_align_to(patient_cand_panel, patient_kb_cn, LV_ALIGN_OUT_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(patient_cand_panel, UI_COLOR_SURFACE, 0);

    patient_kb_toggle_btn = lv_btn_create(patient_overlay);
    lv_obj_set_size(patient_kb_toggle_btn, 56, 36);
    lv_obj_align_to(patient_kb_toggle_btn, patient_kb_cn, LV_ALIGN_OUT_TOP_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(patient_kb_toggle_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(patient_kb_toggle_btn, 8, 0);
    lv_obj_add_event_cb(patient_kb_toggle_btn, patient_kb_toggle_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(patient_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * toggle_lbl = lv_label_create(patient_kb_toggle_btn);
    lv_label_set_text(toggle_lbl, "EN");
    UI_SET_FONT(toggle_lbl);
    lv_obj_set_style_text_color(toggle_lbl, lv_color_white(), 0);
    lv_obj_align(toggle_lbl, LV_ALIGN_CENTER, 0, 0);

    lang = lang_get_current();
    patient_kb_lang = (strncmp(lang, "zh", 2) == 0) ? 0 : 1;
}

static void patient_dialog_event_cb(lv_event_t * e)
{
    int btn_id = (int)(intptr_t)lv_event_get_user_data(e);

    if(btn_id == 1) {
        const char * id = lv_textarea_get_text(txt_id);
        const char * name = lv_textarea_get_text(txt_name);
        const char * age = lv_textarea_get_text(txt_age);
        char gender[16] = {0};
        if(dd_gender) {
            lv_dropdown_get_selected_str(dd_gender, gender, sizeof(gender));
        }

        endoscope_save_patient_info(id, name, gender, age);
        endoscope_main_update_patient_info();
    }

    if(patient_overlay) {
        lv_obj_del(patient_overlay);
        patient_overlay = NULL;
        patient_dialog = NULL;
        txt_id = NULL;
        txt_name = NULL;
        dd_gender = NULL;
        txt_age = NULL;
        patient_kb_cn = NULL;
        patient_kb_en = NULL;
        patient_kb_toggle_btn = NULL;
        patient_cand_panel = NULL;
        patient_ime = NULL;
        patient_kb_current_ta = NULL;
    }
    g_dialog_showing = 0;
}

void endoscope_patient_dialog_show(void)
{
    if(!patient_overlay) {
        create_patient_dialog();
    }
}

void endoscope_patient_dialog_hide(void)
{
    if(patient_overlay) {
        lv_obj_del(patient_overlay);
        patient_overlay = NULL;
        patient_dialog = NULL;
        txt_id = NULL;
        txt_name = NULL;
        dd_gender = NULL;
        txt_age = NULL;
        patient_kb_cn = NULL;
        patient_kb_en = NULL;
        patient_kb_toggle_btn = NULL;
        patient_cand_panel = NULL;
        patient_ime = NULL;
        patient_kb_current_ta = NULL;
    }
    g_dialog_showing = 0;
}

void endoscope_save_patient_info(const char *id, const char *name, const char *gender, const char *age)
{
    if(!id || !name || !gender || !age) return;

    size_t id_len = strlen(id);
    size_t name_len = strlen(name);
    size_t gender_len = strlen(gender);
    size_t age_len = strlen(age);

    if (id_len >= sizeof(status.patient.id) ||
        name_len >= sizeof(status.patient.name) ||
        gender_len >= sizeof(status.patient.gender) ||
        age_len >= sizeof(status.patient.age)) {
        LV_LOG_ERROR("Patient info too long");
        return;
    }

    memcpy(status.patient.id, id, id_len + 1);
    memcpy(status.patient.name, name, name_len + 1);
    memcpy(status.patient.gender, gender, gender_len + 1);
    memcpy(status.patient.age, age, age_len + 1);
    status.patient.has_data = true;

    LV_LOG_USER("Patient info saved: %s, %s, %s, %s", id, name, gender, age);
}

patient_info_t * endoscope_get_patient_info(void)
{
    return &status.patient;
}

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "src/libs/lodepng/lodepng.h"
#include "display_config.h"
#include "hi3519_port/lv_port_disp.h"

int endoscope_ui_snapshot_save(const char * path)
{
    const char * save_path = path ? path : ".";

    struct stat st = {0};
    if(stat(save_path, &st) == -1) {
        mkdir(save_path, 0777);
    }

    static int snapshot_counter = 0;
    snapshot_counter++;

    char bmp_filename[256];
    snprintf(bmp_filename, sizeof(bmp_filename), "%s/ui_snapshot_%03d.bmp",
             save_path, snapshot_counter);

    /* 通过显示驱动获取 framebuffer 数据，避免重复 open fb0 */
    uint32_t w = 0, h = 0, stride = 0;
    int bpp = 0;
    size_t max_size = 1920 * 1080 * 4;
    uint8_t * fb_data = (uint8_t *)malloc(max_size);
    if(!fb_data) {
        LV_LOG_ERROR("Failed to allocate snapshot buffer");
        return -1;
    }

    if(lv_port_disp_snapshot(fb_data, max_size, &w, &h, &stride, &bpp) < 0) {
        LV_LOG_ERROR("Failed to get framebuffer snapshot");
        free(fb_data);
        return -1;
    }

    LV_LOG_USER("Framebuffer: %dx%d, bpp: %d, stride: %d", w, h, bpp, stride);

    FILE * fp = fopen(bmp_filename, "wb");
    if(!fp) {
        LV_LOG_ERROR("Failed to create file: %s", bmp_filename);
        free(fb_data);
        return -1;
    }

    const uint32_t row_size = w * 4;
    const uint32_t pixel_data_size = row_size * h;
    const uint32_t file_size = 14 + 40 + pixel_data_size;

    /* BMP 文件头 (14 bytes) */
    uint8_t file_header[14] = {
        'B', 'M',
        (uint8_t)(file_size & 0xFF), (uint8_t)((file_size >> 8) & 0xFF),
        (uint8_t)((file_size >> 16) & 0xFF), (uint8_t)((file_size >> 24) & 0xFF),
        0, 0, 0, 0,
        54, 0, 0, 0
    };

    /* DIB 头 BITMAPINFOHEADER (40 bytes) */
    uint8_t dib_header[40] = {
        40, 0, 0, 0,
        (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF), (uint8_t)((w >> 16) & 0xFF), (uint8_t)((w >> 24) & 0xFF),
        (uint8_t)(h & 0xFF), (uint8_t)((h >> 8) & 0xFF), (uint8_t)((h >> 16) & 0xFF), (uint8_t)((h >> 24) & 0xFF),
        1, 0,
        32, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    fwrite(file_header, 1, 14, fp);
    fwrite(dib_header, 1, 40, fp);

    /* BMP 像素数据从底向上写入 */
    for(int32_t y = (int32_t)h - 1; y >= 0; y--) {
        fwrite(fb_data + y * stride, 1, row_size, fp);
    }

    fclose(fp);
    free(fb_data);

    LV_LOG_USER("UI snapshot saved to %s", bmp_filename);
    return 0;
}
