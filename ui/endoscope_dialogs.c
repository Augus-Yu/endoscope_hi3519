/**
 * @file endoscope_dialogs.c
 * @brief 提示弹窗实现
 */

#include <string.h>
#include "endoscope_dialogs.h"
#include "endoscope_ui.h"
#include "font_manager.h"
#include "lang_manager.h"
#include "ui_helpers.h"
#include "src/widgets/keyboard/lv_keyboard.h"
#include "src/others/ime/lv_ime_pinyin.h"

extern volatile int g_dialog_showing;

static lv_obj_t * dialog_overlay = NULL;
static lv_obj_t * dialog_box = NULL;
static endoscope_dialog_confirm_cb_t confirm_callback = NULL;

/* 密码对话框相关 */
static lv_obj_t * pwd_inputs[3] = {NULL};
static lv_obj_t * pwd_error_lbl = NULL;
static endoscope_dialog_password_cb_t password_callback = NULL;
static char current_password[32] = {0};
static lv_obj_t * pwd_kb_en = NULL;
static lv_obj_t * pwd_kb_cn = NULL;
static lv_obj_t * pwd_ime = NULL;
static lv_obj_t * pwd_cand_panel = NULL;
static lv_obj_t * pwd_kb_toggle_btn = NULL;
static lv_obj_t * pwd_kb_current_ta = NULL;
static int pwd_kb_lang = 0;

static void close_btn_event(lv_event_t * e);
static void confirm_btn_event(lv_event_t * e);
static void password_confirm_btn_event(lv_event_t * e);
static void password_cancel_btn_event(lv_event_t * e);
static void pwd_kb_event_cb(lv_event_t * e);
static void pwd_ta_event_cb(lv_event_t * e);
static void pwd_kb_toggle_event_cb(lv_event_t * e);
static void pwd_kb_switch_language(void);

static void pwd_kb_switch_language(void)
{
    pwd_kb_lang = !pwd_kb_lang;

    if(pwd_kb_lang == 0) {
        if(pwd_kb_en) lv_obj_add_flag(pwd_kb_en, LV_OBJ_FLAG_HIDDEN);
        if(pwd_kb_cn) {
            lv_obj_remove_flag(pwd_kb_cn, LV_OBJ_FLAG_HIDDEN);
            if(pwd_kb_current_ta) lv_keyboard_set_textarea(pwd_kb_cn, pwd_kb_current_ta);
        }
        if(pwd_cand_panel) lv_obj_remove_flag(pwd_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(pwd_kb_toggle_btn) lv_label_set_text(lv_obj_get_child(pwd_kb_toggle_btn, 0), "EN");
    } else {
        if(pwd_kb_cn) lv_obj_add_flag(pwd_kb_cn, LV_OBJ_FLAG_HIDDEN);
        if(pwd_kb_en) {
            lv_obj_remove_flag(pwd_kb_en, LV_OBJ_FLAG_HIDDEN);
            if(pwd_kb_current_ta) lv_keyboard_set_textarea(pwd_kb_en, pwd_kb_current_ta);
        }
        if(pwd_cand_panel) lv_obj_add_flag(pwd_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(pwd_kb_toggle_btn) lv_label_set_text(lv_obj_get_child(pwd_kb_toggle_btn, 0), "中");
    }
}

static void pwd_kb_toggle_event_cb(lv_event_t * e)
{
    (void)e;
    pwd_kb_switch_language();
}

void endoscope_dialogs_init(void)
{
    /* 初始化时不创建，需要时动态创建 */
}

void endoscope_dialogs_show(const char *title, const char *msg, const char *btn_text)
{
    if(dialog_overlay) {
        endoscope_dialogs_hide();
    }
    g_dialog_showing = 1;
    
    /* 创建遮罩层 */
    dialog_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(dialog_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dialog_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(dialog_overlay, LV_OBJ_FLAG_CLICKABLE);
    
    /* 创建对话框 */
    dialog_box = lv_obj_create(dialog_overlay);
    lv_obj_set_size(dialog_box, 400, 200);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(dialog_box, 10, 0);
    lv_obj_set_style_shadow_width(dialog_box, 20, 0);
    lv_obj_set_style_shadow_color(dialog_box, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(dialog_box, LV_OPA_30, 0);

    /* 标题 */
    lv_obj_t * title_lbl = lv_label_create(dialog_box);
    lv_label_set_text(title_lbl, title);
    UI_SET_FONT(title_lbl);
    lv_obj_set_style_text_color(title_lbl, UI_COLOR_TEXT, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 20);

    /* 消息内容 */
    lv_obj_t * msg_lbl = lv_label_create(dialog_box);
    lv_label_set_text(msg_lbl, msg);
    UI_SET_FONT(msg_lbl);
    lv_obj_set_style_text_color(msg_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    lv_label_set_long_mode(msg_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_lbl, 360);
    lv_obj_align(msg_lbl, LV_ALIGN_CENTER, 0, 0);

    /* 确定按钮 */
    lv_obj_t * btn = lv_btn_create(dialog_box);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, close_btn_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, btn_text ? btn_text : "OK");
    UI_SET_FONT(btn_lbl);
    lv_obj_set_style_text_color(btn_lbl, lv_color_white(), 0);
    lv_obj_align(btn_lbl, LV_ALIGN_CENTER, 0, 0);
}

void endoscope_dialogs_hide(void)
{
    if(dialog_overlay) {
        lv_obj_delete(dialog_overlay);
        dialog_overlay = NULL;
        dialog_box = NULL;
        confirm_callback = NULL;
        for(int i = 0; i < 3; i++) {
            pwd_inputs[i] = NULL;
        }
        pwd_error_lbl = NULL;
        pwd_kb_en = NULL;
        pwd_kb_cn = NULL;
        pwd_ime = NULL;
        pwd_cand_panel = NULL;
        pwd_kb_toggle_btn = NULL;
        pwd_kb_current_ta = NULL;
    }
    g_dialog_showing = 0;
}

/**
 * @brief 显示双按钮确认对话框
 */
void endoscope_dialogs_confirm(const char *title, const char *msg,
                               const char *yes_text, const char *no_text,
                               endoscope_dialog_confirm_cb_t callback)
{
    confirm_callback = callback;

    if(dialog_overlay) {
        endoscope_dialogs_hide();
    }
    g_dialog_showing = 1;

    /* 创建遮罩层 */
    dialog_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(dialog_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dialog_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(dialog_overlay, LV_OBJ_FLAG_CLICKABLE);

    /* 创建对话框 - 增大尺寸 */
    dialog_box = lv_obj_create(dialog_overlay);
    lv_obj_set_size(dialog_box, 600, 440);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(dialog_box, 16, 0);
    lv_obj_set_style_shadow_width(dialog_box, 24, 0);
    lv_obj_set_style_shadow_color(dialog_box, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(dialog_box, LV_OPA_40, 0);
    lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dialog_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dialog_box, 40, 0);
    lv_obj_set_style_pad_row(dialog_box, 20, 0);

    /* 内容容器 - 包含标题和消息，在剩余空间中居中 */
    lv_obj_t * content_cont = lv_obj_create(dialog_box);
    lv_obj_set_size(content_cont, 520, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(content_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(content_cont, 0, 0);
    lv_obj_clear_flag(content_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content_cont, 0, 0);
    lv_obj_set_style_pad_row(content_cont, 20, 0);
    lv_obj_set_flex_grow(content_cont, 1);

    /* 标题 - 完全居中 */
    lv_obj_t * title_lbl = lv_label_create(content_cont);
    lv_label_set_text(title_lbl, title);
    UI_SET_FONT(title_lbl);
    lv_obj_set_style_text_color(title_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_width(title_lbl, 480);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* 消息内容 - 完全居中 */
    lv_obj_t * msg_lbl = lv_label_create(content_cont);
    lv_label_set_text(msg_lbl, msg);
    lv_obj_set_style_text_color(msg_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    UI_SET_FONT(msg_lbl);
    lv_label_set_long_mode(msg_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_lbl, 480);
    lv_obj_set_style_text_align(msg_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* 按钮容器 */
    lv_obj_t * btn_cont = lv_obj_create(dialog_box);
    lv_obj_set_size(btn_cont, 480, 56);
    lv_obj_set_style_bg_color(btn_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_cont, 80, 0);

    /* "是"按钮 - 蓝色 */
    lv_obj_t * yes_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(yes_btn, 140, 44);
    lv_obj_set_style_bg_color(yes_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(yes_btn, 22, 0);
    lv_obj_set_style_pad_all(yes_btn, 0, 0);
    lv_obj_add_event_cb(yes_btn, confirm_btn_event, LV_EVENT_CLICKED, (void *)1);

    lv_obj_t * yes_lbl = lv_label_create(yes_btn);
    lv_label_set_text(yes_lbl, yes_text ? yes_text : "Yes");
    UI_SET_FONT(yes_lbl);
    lv_obj_set_style_text_color(yes_lbl, lv_color_white(), 0);
    lv_obj_align(yes_lbl, LV_ALIGN_CENTER, 0, 0);

    /* "否"按钮 - 灰色 */
    lv_obj_t * no_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(no_btn, 140, 44);
    lv_obj_set_style_bg_color(no_btn, lv_color_hex(0x5a6a7a), 0);
    lv_obj_set_style_radius(no_btn, 22, 0);
    lv_obj_set_style_pad_all(no_btn, 0, 0);
    lv_obj_add_event_cb(no_btn, confirm_btn_event, LV_EVENT_CLICKED, (void *)0);

    lv_obj_t * no_lbl = lv_label_create(no_btn);
    lv_label_set_text(no_lbl, no_text ? no_text : "No");
    UI_SET_FONT(no_lbl);
    lv_obj_set_style_text_color(no_lbl, lv_color_white(), 0);
    lv_obj_align(no_lbl, LV_ALIGN_CENTER, 0, 0);
}

/**
 * @brief 显示带图标的成功对话框
 */
void endoscope_dialogs_success(const char *title, const char *msg, const char *btn_text)
{
    if(dialog_overlay) {
        endoscope_dialogs_hide();
    }
    g_dialog_showing = 1;

    dialog_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(dialog_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dialog_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(dialog_overlay, LV_OBJ_FLAG_CLICKABLE);

    /* 创建对话框 - 统一尺寸 */
    dialog_box = lv_obj_create(dialog_overlay);
    lv_obj_set_size(dialog_box, 600, 440);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(dialog_box, 16, 0);
    lv_obj_set_style_shadow_width(dialog_box, 24, 0);
    lv_obj_set_style_shadow_color(dialog_box, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(dialog_box, LV_OPA_40, 0);
    lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dialog_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dialog_box, 40, 0);
    lv_obj_set_style_pad_row(dialog_box, 20, 0);

    /* 内容容器 - 包含图标、标题和消息，在剩余空间中居中 */
    lv_obj_t * content_cont = lv_obj_create(dialog_box);
    lv_obj_set_size(content_cont, 520, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(content_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(content_cont, 0, 0);
    lv_obj_clear_flag(content_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content_cont, 0, 0);
    lv_obj_set_style_pad_row(content_cont, 20, 0);
    lv_obj_set_flex_grow(content_cont, 1);

    /* 绿色勾选图标 - 使用 Unicode 勾选符号 */
    lv_obj_t * icon_lbl = lv_label_create(content_cont);
    lv_label_set_text(icon_lbl, "\xE2\x9C\x93"); /* Unicode checkmark UTF-8 */
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon_lbl, UI_COLOR_SUCCESS, 0);

    /* 标题 - 完全居中 */
    lv_obj_t * title_lbl = lv_label_create(content_cont);
    lv_label_set_text(title_lbl, title);
    UI_SET_FONT(title_lbl);
    lv_obj_set_style_text_color(title_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_width(title_lbl, 480);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* 消息内容 - 完全居中 */
    lv_obj_t * msg_lbl = lv_label_create(content_cont);
    lv_label_set_text(msg_lbl, msg);
    lv_obj_set_style_text_color(msg_lbl, UI_COLOR_TEXT_SECONDARY, 0);
    UI_SET_FONT(msg_lbl);
    lv_label_set_long_mode(msg_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_lbl, 480);
    lv_obj_set_style_text_align(msg_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* 按钮容器 */
    lv_obj_t * btn_cont = lv_obj_create(dialog_box);
    lv_obj_set_size(btn_cont, 480, 64);
    lv_obj_set_style_bg_color(btn_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 确定按钮 */
    lv_obj_t * btn = lv_btn_create(btn_cont);
    lv_obj_set_size(btn, 140, 48);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn, 24, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, close_btn_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, btn_text ? btn_text : "OK");
    UI_SET_FONT(btn_lbl);
    lv_obj_set_style_text_color(btn_lbl, lv_color_white(), 0);
    lv_obj_align(btn_lbl, LV_ALIGN_CENTER, 0, 0);
}

static void close_btn_event(lv_event_t * e)
{
    (void)e;
    endoscope_dialogs_hide();
}

static void confirm_btn_event(lv_event_t * e)
{
    bool confirmed = (bool)(intptr_t)lv_event_get_user_data(e);

    endoscope_dialogs_hide();

    if(confirm_callback) {
        confirm_callback(confirmed);
        confirm_callback = NULL;
    }
}

static void pwd_kb_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        if(pwd_kb_en) lv_obj_add_flag(pwd_kb_en, LV_OBJ_FLAG_HIDDEN);
        if(pwd_kb_cn) lv_obj_add_flag(pwd_kb_cn, LV_OBJ_FLAG_HIDDEN);
        if(pwd_cand_panel) lv_obj_add_flag(pwd_cand_panel, LV_OBJ_FLAG_HIDDEN);
        if(pwd_kb_toggle_btn) lv_obj_add_flag(pwd_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);
        lv_indev_reset(NULL, NULL);
    }
}

static void pwd_ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target_obj(e);
    if(code == LV_EVENT_FOCUSED) {
        pwd_kb_current_ta = ta;
        if(pwd_kb_toggle_btn) lv_obj_remove_flag(pwd_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);

        if(pwd_kb_lang == 0) {
            if(pwd_kb_en) lv_obj_add_flag(pwd_kb_en, LV_OBJ_FLAG_HIDDEN);
            if(pwd_kb_cn) {
                lv_keyboard_set_textarea(pwd_kb_cn, ta);
                lv_obj_remove_flag(pwd_kb_cn, LV_OBJ_FLAG_HIDDEN);
            }
            if(pwd_cand_panel) lv_obj_remove_flag(pwd_cand_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            if(pwd_kb_cn) lv_obj_add_flag(pwd_kb_cn, LV_OBJ_FLAG_HIDDEN);
            if(pwd_kb_en) {
                lv_keyboard_set_textarea(pwd_kb_en, ta);
                lv_obj_remove_flag(pwd_kb_en, LV_OBJ_FLAG_HIDDEN);
            }
            if(pwd_cand_panel) lv_obj_add_flag(pwd_cand_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

bool endoscope_dialogs_verify_password(const char *password)
{
    if(!password) return false;
    return (strcmp(current_password, password) == 0);
}

/**
 * @brief 设置/更新密码
 */
bool endoscope_dialogs_set_password(const char *new_password)
{
    if(!new_password) {
        return false;
    }
    size_t len = strlen(new_password);
    if(len < 6 || len >= sizeof(current_password)) {
        return false;
    }
    memcpy(current_password, new_password, len + 1);
    return true;
}

/**
 * @brief 显示修改密码对话框
 */
void endoscope_dialogs_password_change(endoscope_dialog_password_cb_t callback)
{
    /* 保存回调函数 */
    password_callback = callback;
    if(dialog_overlay) {
        endoscope_dialogs_hide();
    }
    g_dialog_showing = 1;
    
    /* 创建遮罩层 */
    dialog_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(dialog_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dialog_overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(dialog_overlay, LV_OBJ_FLAG_CLICKABLE);
    
    /* 创建对话框 - 统一尺寸 */
    dialog_box = lv_obj_create(dialog_overlay);
    lv_obj_set_size(dialog_box, 600, 440);
    lv_obj_center(dialog_box);
    lv_obj_set_style_bg_color(dialog_box, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(dialog_box, 16, 0);
    lv_obj_set_style_shadow_width(dialog_box, 24, 0);
    lv_obj_set_style_shadow_color(dialog_box, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(dialog_box, LV_OPA_40, 0);
    lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dialog_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dialog_box, 30, 0);
    lv_obj_set_style_pad_row(dialog_box, 12, 0);

    /* 内容容器 - 包含标题和输入框，在剩余空间中居中 */
    lv_obj_t * content_cont = lv_obj_create(dialog_box);
    lv_obj_set_size(content_cont, 520, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(content_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(content_cont, 0, 0);
    lv_obj_clear_flag(content_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(content_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content_cont, 0, 0);
    lv_obj_set_style_pad_row(content_cont, 16, 0);
    lv_obj_set_flex_grow(content_cont, 1);

    /* 标题 - 完全居中 */
    lv_obj_t * title_lbl = lv_label_create(content_cont);
    lv_label_set_text(title_lbl, _TR("DLG_CHANGE_PASSWORD_TITLE"));
    UI_SET_FONT(title_lbl);
    lv_obj_set_style_text_color(title_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_width(title_lbl, 480);
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* 三个密码输入框 */
    const char * label_keys[3] = {
        "DLG_OLD_PASSWORD",
        "DLG_NEW_PASSWORD",
        "DLG_CONFIRM_PASSWORD"
    };

    for(int i = 0; i < 3; i++) {
        /* 每行的容器 */
        lv_obj_t * row_cont = lv_obj_create(content_cont);
        lv_obj_set_size(row_cont, 520, 52);
        lv_obj_set_style_bg_color(row_cont, UI_COLOR_SURFACE, 0);
        lv_obj_set_style_border_width(row_cont, 0, 0);
        lv_obj_clear_flag(row_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row_cont, 16, 0);

        /* 标签 */
        lv_obj_t * lbl = lv_label_create(row_cont);
        lv_label_set_text(lbl, _TR(label_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT, 0);
        lv_obj_set_width(lbl, 110);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_RIGHT, 0);

        /* 输入框 */
        pwd_inputs[i] = lv_textarea_create(row_cont);
        lv_obj_set_size(pwd_inputs[i], 390, 44);
        lv_textarea_set_one_line(pwd_inputs[i], true);
        lv_textarea_set_password_mode(pwd_inputs[i], true);
        lv_obj_set_style_bg_color(pwd_inputs[i], lv_color_hex(0x1e2d4d), 0);
        lv_obj_set_style_text_color(pwd_inputs[i], UI_COLOR_TEXT, 0);
        lv_obj_set_style_border_width(pwd_inputs[i], 0, 0);
        lv_obj_set_style_radius(pwd_inputs[i], 8, 0);
        lv_obj_set_style_pad_hor(pwd_inputs[i], 16, 0);
        UI_SET_FONT(pwd_inputs[i]);
        lv_textarea_set_placeholder_text(pwd_inputs[i], "******");
        lv_obj_add_event_cb(pwd_inputs[i], pwd_ta_event_cb, LV_EVENT_FOCUSED, NULL);
    }

    pwd_ime = lv_ime_pinyin_create(dialog_overlay);
    lv_obj_set_style_text_font(pwd_ime, font_manager_get_font(), 0);

    /* 替换为 20924 汉字大词库 */
    extern const lv_pinyin_dict_t lv_ime_pinyin_large_dict[];
    lv_ime_pinyin_set_dict(pwd_ime, (lv_pinyin_dict_t *)lv_ime_pinyin_large_dict);

    pwd_kb_en = lv_keyboard_create(dialog_overlay);
    lv_obj_set_size(pwd_kb_en, LV_HOR_RES, 280);
    lv_obj_align(pwd_kb_en, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(pwd_kb_en, NULL);
    lv_obj_add_flag(pwd_kb_en, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(pwd_kb_en, pwd_kb_event_cb, LV_EVENT_ALL, NULL);

    pwd_kb_cn = lv_keyboard_create(dialog_overlay);
    lv_obj_set_size(pwd_kb_cn, LV_HOR_RES, 280);
    lv_obj_align(pwd_kb_cn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(pwd_kb_cn, NULL);
    lv_obj_add_flag(pwd_kb_cn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(pwd_kb_cn, pwd_kb_event_cb, LV_EVENT_ALL, NULL);
    lv_ime_pinyin_set_keyboard(pwd_ime, pwd_kb_cn);

    pwd_cand_panel = lv_ime_pinyin_get_cand_panel(pwd_ime);
    lv_font_t *cand_font = font_manager_get_font();
    if (cand_font) {
        lv_obj_set_style_text_font(pwd_cand_panel, cand_font, LV_PART_MAIN);
        lv_obj_set_style_text_font(pwd_cand_panel, cand_font, LV_PART_ITEMS);
    }
    lv_obj_set_size(pwd_cand_panel, LV_HOR_RES, 48);
    lv_obj_align_to(pwd_cand_panel, pwd_kb_cn, LV_ALIGN_OUT_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(pwd_cand_panel, UI_COLOR_SURFACE, 0);

    pwd_kb_toggle_btn = lv_btn_create(dialog_overlay);
    lv_obj_set_size(pwd_kb_toggle_btn, 56, 36);
    lv_obj_align_to(pwd_kb_toggle_btn, pwd_kb_cn, LV_ALIGN_OUT_TOP_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(pwd_kb_toggle_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(pwd_kb_toggle_btn, 8, 0);
    lv_obj_add_event_cb(pwd_kb_toggle_btn, pwd_kb_toggle_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(pwd_kb_toggle_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * pwd_toggle_lbl = lv_label_create(pwd_kb_toggle_btn);
    lv_label_set_text(pwd_toggle_lbl, "EN");
    UI_SET_FONT(pwd_toggle_lbl);
    lv_obj_set_style_text_color(pwd_toggle_lbl, lv_color_white(), 0);
    lv_obj_align(pwd_toggle_lbl, LV_ALIGN_CENTER, 0, 0);

    const char * pwd_lang = lang_get_current();
    pwd_kb_lang = (strncmp(pwd_lang, "zh", 2) == 0) ? 0 : 1;

    /* 错误提示标签 */
    pwd_error_lbl = lv_label_create(content_cont);
    lv_label_set_text(pwd_error_lbl, "");
    UI_SET_FONT(pwd_error_lbl);
    lv_obj_set_style_text_color(pwd_error_lbl, UI_COLOR_ERROR, 0);
    lv_obj_set_height(pwd_error_lbl, 20);
    lv_obj_set_style_text_align(pwd_error_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* 按钮容器 */
    lv_obj_t * btn_cont = lv_obj_create(dialog_box);
    lv_obj_set_size(btn_cont, 480, 60);
    lv_obj_set_style_bg_color(btn_cont, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_cont, 80, 0);

    /* "确定"按钮 - 蓝色，圆角 */
    lv_obj_t * ok_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(ok_btn, 140, 48);
    lv_obj_set_style_bg_color(ok_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(ok_btn, 24, 0);
    lv_obj_set_style_pad_all(ok_btn, 0, 0);
    lv_obj_add_event_cb(ok_btn, password_confirm_btn_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, _TR("DLG_BTN_OK"));
    UI_SET_FONT(ok_lbl);
    lv_obj_set_style_text_color(ok_lbl, lv_color_white(), 0);
    lv_obj_align(ok_lbl, LV_ALIGN_CENTER, 0, 0);

    /* "取消"按钮 - 灰色，圆角 */
    lv_obj_t * cancel_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(cancel_btn, 140, 48);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x5a6a7a), 0);
    lv_obj_set_style_radius(cancel_btn, 24, 0);
    lv_obj_set_style_pad_all(cancel_btn, 0, 0);
    lv_obj_add_event_cb(cancel_btn, password_cancel_btn_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t * cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, _TR("DLG_CANCEL"));
    UI_SET_FONT(cancel_lbl);
    lv_obj_set_style_text_color(cancel_lbl, lv_color_white(), 0);
    lv_obj_align(cancel_lbl, LV_ALIGN_CENTER, 0, 0);
}

static void password_confirm_btn_event(lv_event_t * e)
{
    (void)e;
    
    const char * old_pwd = lv_textarea_get_text(pwd_inputs[0]);
    const char * new_pwd = lv_textarea_get_text(pwd_inputs[1]);
    const char * confirm_pwd = lv_textarea_get_text(pwd_inputs[2]);
    
    /* 验证旧密码 */
    if(!endoscope_dialogs_verify_password(old_pwd)) {
        lv_label_set_text(pwd_error_lbl, _TR("DLG_PASSWORD_WRONG"));
        return;
    }
    
    /* 验证新密码长度 */
    if(strlen(new_pwd) < 6) {
        lv_label_set_text(pwd_error_lbl, _TR("DLG_PASSWORD_TOO_SHORT"));
        return;
    }
    
    /* 验证两次输入是否一致 */
    if(strcmp(new_pwd, confirm_pwd) != 0) {
        lv_label_set_text(pwd_error_lbl, _TR("DLG_PASSWORD_NOT_MATCH"));
        return;
    }
    
    /* 验证通过，关闭对话框并调用回调 */
    endoscope_dialogs_hide();
    
    if(password_callback) {
        password_callback(old_pwd, new_pwd);
        password_callback = NULL;
    }
}

static void password_cancel_btn_event(lv_event_t * e)
{
    (void)e;
    endoscope_dialogs_hide();
    password_callback = NULL;
}
