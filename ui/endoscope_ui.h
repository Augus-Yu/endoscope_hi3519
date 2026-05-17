/**
 * @file endoscope_ui.h
 * @brief 医疗电子内窥镜UI主头文件
 */

#ifndef ENDOSCOPE_UI_H
#define ENDOSCOPE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_theme.h"

/*********************
 *      DEFINES
 *********************/

#define DISPLAY_WIDTH   1920
#define DISPLAY_HEIGHT  1080

/*********************
 *      TYPES
 *********************/

typedef enum {
    ENDOSCOPE_SCREEN_SPLASH,
    ENDOSCOPE_SCREEN_MAIN,
    ENDOSCOPE_SCREEN_SETTINGS,
    ENDOSCOPE_SCREEN_PLAYBACK,
    ENDOSCOPE_SCREEN_IMAGE_SETTINGS,
    ENDOSCOPE_SCREEN_PLAYER,
} endoscope_screen_t;

typedef struct {
    char id[32];
    char name[64];
    char gender[8];
    char age[8];
    bool has_data;
} patient_info_t;

typedef struct {
    bool endoscope_connected;
    bool usb_connected;
    bool is_recording;
    bool is_capturing;
    uint32_t recording_time;
    uint8_t battery_level;
    uint8_t storage_percent;
    patient_info_t patient;
} endoscope_status_t;

/**********************
 * GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化内窥镜UI系统
 */
void endoscope_ui_init(void);

/**
 * @brief 获取当前屏幕
 */
endoscope_screen_t endoscope_get_current_screen(void);

/**
 * @brief 获取系统状态
 */
endoscope_status_t * endoscope_get_status(void);

/**
 * @brief 设置内窥镜连接状态
 */
void endoscope_set_endoscope_connected(bool connected);

/**
 * @brief 设置U盘连接状态
 */
void endoscope_set_usb_connected(bool connected);

/**
 * @brief 显示/隐藏提示弹窗
 */
void endoscope_show_dialog(const char *title, const char *msg, const char *btn_text);
void endoscope_hide_dialog(void);

/**
 * @brief 显示病人信息录入对话框
 */
void endoscope_patient_dialog_show(void);

/**
 * @brief 隐藏病人信息录入对话框
 */
void endoscope_patient_dialog_hide(void);

/**
 * @brief 更新主界面的病人信息显示
 */
void endoscope_main_update_patient_info(void);

/**
 * @brief 保存病人信息
 */
void endoscope_save_patient_info(const char *id, const char *name, const char *gender, const char *age);

/**
 * @brief 获取当前病人信息
 */
patient_info_t * endoscope_get_patient_info(void);

/**
 * @brief 保存当前UI界面截图到指定路径
 * @param path 保存路径，为NULL则使用默认路径 /opt/endoscope/ui_snapshot/
 * @return 0成功，-1失败
 */
int endoscope_ui_snapshot_save(const char * path);

#ifdef __cplusplus
}
#endif

#endif /* ENDOSCOPE_UI_H */
