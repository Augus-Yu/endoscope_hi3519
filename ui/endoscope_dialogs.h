/**
 * @file endoscope_dialogs.h
 * @brief 提示弹窗
 */

#ifndef ENDOSCOPE_DIALOGS_H
#define ENDOSCOPE_DIALOGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void endoscope_dialogs_init(void);
void endoscope_dialogs_show(const char *title, const char *msg, const char *btn_text);
void endoscope_dialogs_hide(void);

/* 双按钮确认对话框回调类型 */
typedef void (*endoscope_dialog_confirm_cb_t)(bool confirmed);

/**
 * @brief 显示双按钮确认对话框
 * @param title 对话框标题
 * @param msg 对话框消息
 * @param yes_text "是"按钮文本
 * @param no_text "否"按钮文本
 * @param callback 用户点击按钮后的回调函数 (true=是, false=否)
 */
void endoscope_dialogs_confirm(const char *title, const char *msg, 
                               const char *yes_text, const char *no_text,
                               endoscope_dialog_confirm_cb_t callback);

/**
 * @brief 显示带图标的成功对话框
 * @param title 对话框标题
 * @param msg 对话框消息
 * @param btn_text 按钮文本
 */
void endoscope_dialogs_success(const char *title, const char *msg, const char *btn_text);

/* 修改密码对话框回调类型 */
typedef void (*endoscope_dialog_password_cb_t)(const char *old_pwd, const char *new_pwd);

/**
 * @brief 显示修改密码对话框
 * @param callback 用户确认后的回调函数，传入旧密码和新密码
 */
void endoscope_dialogs_password_change(endoscope_dialog_password_cb_t callback);

/**
 * @brief 验证密码
 * @param password 要验证的密码
 * @return true 密码正确, false 密码错误
 */
bool endoscope_dialogs_verify_password(const char *password);

/**
 * @brief 设置/更新密码
 * @param new_password 新密码
 * @return true 设置成功, false 设置失败
 */
bool endoscope_dialogs_set_password(const char *new_password);

#ifdef __cplusplus
}
#endif

#endif
