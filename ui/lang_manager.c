/**
 * @file lang_manager.c
 * @brief 嵌入式多语言管理器 - 硬编码翻译
 */

#include "lang_manager.h"
#include <stdio.h>
#include <string.h>

/* 翻译条目 */
typedef struct {
    const char * key;
    const char * text;
} translation_entry_t;

/* 英文翻译 - 默认 */
static const translation_entry_t en_translations[] = {
    /* 通用按钮 */
    {"DLG_OK", "OK"},
    {"DLG_CANCEL", "Cancel"},
    {"DLG_CLOSE", "Close"},
    {"DLG_SAVE", "Save"},
    {"DLG_BACK", "Back"},
    {"DLG_BTN_OK", "OK"},
    {"DLG_BTN_YES", "Yes"},
    {"DLG_BTN_NO", "No"},
    
    /* 启动画面 */
    {"SPLASH_TITLE", "Endoscope System"},
    
    /* 主界面 */
    {"MAIN_TITLE", "Endoscope System"},
    {"MAIN_DATE_PREFIX", "Date:"},
    {"MAIN_PATIENT_INFO", "Patient Info"},
    {"MAIN_PATIENT_ID", "ID:"},
    {"MAIN_PATIENT_NAME", "Name:"},
    {"MAIN_PATIENT_GENDER", "Gender:"},
    {"MAIN_PATIENT_AGE", "Age:"},
    {"MAIN_ENDOSCOPE_TYPE", "Endoscope Type"},
    {"MAIN_ENDOSCOPE_ID", "Device ID"},
    {"MAIN_WORK_MODE", "Mode"},
    {"MAIN_USB_INFO", "USB Status"},
    {"MAIN_RECORDING", "Recording..."},
    {"MAIN_LED_BRIGHTNESS", "LED Brightness"},
    
    /* 主界面按钮 */
    {"MAIN_BTN_CAPTURE", "Capture"},
    {"MAIN_BTN_RECORD", "Record"},
    {"MAIN_BTN_STOP", "Stop"},
    {"MAIN_BTN_PLAYBACK", "Playback"},
    {"MAIN_BTN_SETTINGS", "Settings"},
    
    /* 右侧按钮 */
    {"BTN_PATIENT", "Patient"},
    {"BTN_SETTINGS", "Settings"},
    {"BTN_BALANCE", "Balance"},
    {"BTN_FREEZE", "Freeze"},
    {"BTN_CAPTURE", "Capture"},
    {"BTN_RECORD", "Record"},
    {"BTN_ZOOM", "Zoom"},
    {"BTN_PLAYBACK", "Playback"},
    
    /* 状态 */
    {"MAIN_STATUS_READY", "Ready"},
    {"MAIN_STATUS_RECORDING", "Recording..."},
    
    /* 设置界面 */
    {"SET_TITLE", "Settings"},
    {"SET_LANGUAGE", "Language"},
    {"SET_IMAGE", "Image Settings"},
    {"SET_STORAGE", "Storage"},
    {"SET_ABOUT", "About"},
    
    /* 系统设置 */
    {"SYS_TIME", "Time"},
    {"SYS_LANGUAGE", "Language"},
    {"DEVICE_NAME", "Device Name"},
    {"KEY_PREFIX", "Key "},
    {"SET_NAV_KEYS", "Key Settings"},
    {"SET_NAV_SYSTEM", "System Settings"},
    {"SET_NAV_ABOUT", "About Device"},
    {"SET_NAV_POWER", "Power Management"},
    {"POWER_SHUTDOWN", "Shutdown"},
    {"POWER_REBOOT", "Reboot"},
    {"KEY_BALANCE", "White Balance"},
    {"KEY_CAPTURE", "Capture"},
    {"KEY_RECORD", "Record"},
    {"KEY_FREEZE", "Freeze"},
    {"KEY_ENHANCE", "Enhance"},
    {"KEY_DENOISE", "Denoise"},
    {"KEY_DEFOG", "Defog"},
    {"KEY_GAMMA", "Gamma"},
    {"SYS_UPGRADE", "System Upgrade"},
    {"SYS_FACTORY_RESET", "Factory Reset"},
    {"SYS_CHANGE_PASSWORD", "Change Password"},
    {"LANG_CHINESE", "Chinese"},
    {"LANG_ENGLISH", "English"},
    {"ABOUT_COMPANY", "Company"},
    {"ABOUT_DEVICE", "Device Name"},
    {"ABOUT_MODEL", "Model"},

    /* 对话框消息 */
    {"DLG_NO_ENDOSCOPE_MSG", "Endoscope not connected"},

    /* 对话框标题 */
    {"DLG_TITLE_PATIENT", "Patient Information"},
    {"DLG_TITLE_BALANCE", "White Balance"},
    {"DLG_TITLE_FREEZE", "Freeze"},
    {"DLG_TITLE_NOTICE", "Notice"},
    {"DLG_TITLE_CAPTURE", "Capture"},
    {"DLG_TITLE_ZOOM", "Zoom"},
    {"DLG_FACTORY_RESET_TITLE", "Factory Reset"},
    {"DLG_CHANGE_PASSWORD_TITLE", "Change Password"},
    {"DLG_UPGRADE_TITLE", "System Upgrade"},
    
    /* 对话框消息 */
    {"DLG_MSG_CALIBRATING", "Calibrating..."},
    {"DLG_MSG_FROZEN", "Image frozen"},
    {"DLG_MSG_INSERT_USB", "Please insert USB drive"},
    {"DLG_MSG_PHOTO_SAVED", "Photo saved"},
    {"DLG_MSG_ZOOM_FUNC", "Zoom function activated"},
    {"DLG_MSG_GO_BACK", "Go back to previous screen?"},
    {"DLG_FACTORY_RESET_MSG", "Reset all settings to default?"},
    {"DLG_UPGRADE_SUCCESS_MSG", "Upgrade successful!"},
    
    /* 密码相关 */
    {"DLG_PASSWORD_WRONG", "Wrong password"},
    {"DLG_PASSWORD_TOO_SHORT", "Password too short"},
    {"DLG_PASSWORD_NOT_MATCH", "Passwords do not match"},
    
    /* 录像回放 */
    {"PLAYBACK_TITLE", "Video Playback"},
    {"PLAYBACK_BACK", "< Back"},
    {"PLAYBACK_REFRESH", "Refresh"},
    {"PLAYBACK_NO_FILES", "No video files"},
    {"PLAYBACK_FILE_PREFIX", "[Video]"},
    {"PLAYBACK_FAILED", "Playback failed"},

    /* 图像设置 */
    {"IMG_SET_TITLE", "Image"},
    {"IMG_SET_BACK", "< Back"},
    {"IMG_BRIGHTNESS", "Brightness"},
    {"IMG_CONTRAST", "Contrast"},
    {"IMG_SATURATION", "Saturation"},
    {"IMG_SHARPNESS", "Sharpness"},

    /* 提示信息 */
    {"MSG_NO_STORAGE", "No storage device"},
    {"MSG_NO_SIGNAL", "No video signal"},
    {"MSG_SAVED", "Saved successfully"},
};

/* 中文翻译 - 使用字体支持的字符 */
static const translation_entry_t zh_translations[] = {
    /* 通用按钮 */
    {"DLG_OK", "确定"},
    {"DLG_CANCEL", "取消"},
    {"DLG_CLOSE", "关闭"},
    {"DLG_SAVE", "保存"},
    {"DLG_BACK", "返回"},
    {"DLG_BTN_OK", "确定"},
    {"DLG_BTN_YES", "是"},
    {"DLG_BTN_NO", "否"},
    
    /* 启动画面 */
    {"SPLASH_TITLE", "内窥镜便携式主机系统"},

    /* 主界面 */
    {"MAIN_TITLE", "内窥镜检查中"},
    {"MAIN_DATE_PREFIX", "日期:"},
    {"MAIN_PATIENT_INFO", "患者信息"},
    {"MAIN_PATIENT_ID", "编号:"},
    {"MAIN_PATIENT_NAME", "姓名:"},
    {"MAIN_PATIENT_GENDER", "性别:"},
    {"MAIN_PATIENT_AGE", "年龄:"},
    {"MAIN_ENDOSCOPE_TYPE", "镜种"},
    {"MAIN_ENDOSCOPE_ID", "内窥镜编号:"},
    {"MAIN_WORK_MODE", "工作模式:"},
    {"MAIN_USB_INFO", "U盘信息"},
    {"MAIN_RECORDING", "录像中"},
    {"MAIN_LED_BRIGHTNESS", "头端LED亮度调节"},
    
    /* 主界面按钮 */
    {"MAIN_BTN_CAPTURE", "拍照"},
    {"MAIN_BTN_RECORD", "录像"},
    {"MAIN_BTN_STOP", "停止"},
    {"MAIN_BTN_PLAYBACK", "回放"},
    {"MAIN_BTN_SETTINGS", "设置"},
    
    /* 右侧按钮 */
    {"BTN_PATIENT", "病人信息录入"},
    {"BTN_SETTINGS", "设置"},
    {"BTN_BALANCE", "白平衡"},
    {"BTN_FREEZE", "冻结"},
    {"BTN_CAPTURE", "拍照"},
    {"BTN_RECORD", "录像"},
    {"BTN_ZOOM", "电子放大"},
    {"BTN_PLAYBACK", "回放"},
    
    /* 状态 */
    {"MAIN_STATUS_READY", "就绪"},
    {"MAIN_STATUS_RECORDING", "录像中..."},
    
    /* 设置界面 */
    {"SET_TITLE", "设置"},
    {"SET_LANGUAGE", "语言"},
    {"SET_IMAGE", "图像设置"},
    {"SET_STORAGE", "存储"},
    {"SET_ABOUT", "关于"},
    
    /* 系统设置 */
    {"SYS_TIME", "时间"},
    {"SYS_LANGUAGE", "语言"},
    {"DEVICE_NAME", "设备名称"},
    {"KEY_PREFIX", "按键"},

    /* 设置导航 */
    {"SET_NAV_KEYS", "按键设置"},
    {"SET_NAV_SYSTEM", "系统设置"},
    {"SET_NAV_ABOUT", "关于设备"},
    {"SET_NAV_POWER", "电源管理"},
    {"POWER_SHUTDOWN", "关机"},
    {"POWER_REBOOT", "重启"},
    {"KEY_BALANCE", "白平衡"},
    {"KEY_CAPTURE", "拍照"},
    {"KEY_RECORD", "录像"},
    {"KEY_FREEZE", "冻结"},
    {"KEY_ENHANCE", "增强"},
    {"KEY_DENOISE", "降噪"},
    {"KEY_DEFOG", "去雾"},
    {"KEY_GAMMA", "伽马"},
    {"LANG_CHINESE", "中文"},
    {"LANG_ENGLISH", "English"},
    {"SYS_UPGRADE", "系统升级"},
    {"SYS_FACTORY_RESET", "恢复出厂"},
    {"SYS_CHANGE_PASSWORD", "修改密码"},
    {"ABOUT_COMPANY", "公司名称"},
    {"ABOUT_DEVICE", "设备名称"},
    {"ABOUT_MODEL", "设备型号"},

    /* 对话框消息 */
    {"DLG_NO_ENDOSCOPE_MSG", "内窥镜未连接"},

    /* 对话框标题 */
    {"DLG_TITLE_PATIENT", "患者资料"},
    {"DLG_TITLE_BALANCE", "白平衡"},
    {"DLG_TITLE_FREEZE", "冻结"},
    {"DLG_TITLE_NOTICE", "提示"},
    {"DLG_TITLE_CAPTURE", "拍照"},
    {"DLG_TITLE_ZOOM", "变焦"},
    {"DLG_FACTORY_RESET_TITLE", "恢复出厂设置"},
    {"DLG_CHANGE_PASSWORD_TITLE", "修改密码"},
    {"DLG_UPGRADE_TITLE", "系统升级"},
    
    /* 对话框消息 */
    {"DLG_MSG_CALIBRATING", "正在校准..."},
    {"DLG_MSG_FROZEN", "图像已冻结"},
    {"DLG_MSG_INSERT_USB", "请插入U盘"},
    {"DLG_MSG_PHOTO_SAVED", "照片已保存"},
    {"DLG_MSG_ZOOM_FUNC", "变焦功能已激活"},
    {"DLG_MSG_GO_BACK", "返回上一界面?"},
    {"DLG_FACTORY_RESET_MSG", "恢复所有设置为默认值?"},
    {"DLG_UPGRADE_SUCCESS_MSG", "升级成功!"},
    
    /* 密码相关 */
    {"DLG_PASSWORD_WRONG", "密码错误"},
    {"DLG_PASSWORD_TOO_SHORT", "密码太短"},
    {"DLG_PASSWORD_NOT_MATCH", "密码不匹配"},
    
    /* 录像回放 */
    {"PLAYBACK_TITLE", "录像回放"},
    {"PLAYBACK_BACK", "< 返回"},
    {"PLAYBACK_REFRESH", "刷新"},
    {"PLAYBACK_NO_FILES", "无录像文件"},
    {"PLAYBACK_FILE_PREFIX", "[视]"},
    {"PLAYBACK_FAILED", "播放失败"},

    /* 图像设置 */
    {"IMG_SET_TITLE", "图像"},
    {"IMG_SET_BACK", "< 返回"},
    {"IMG_BRIGHTNESS", "亮度"},
    {"IMG_CONTRAST", "对比度"},
    {"IMG_SATURATION", "饱和度"},
    {"IMG_SHARPNESS", "锐度"},

    /* 提示信息 */
    {"MSG_NO_STORAGE", "无存储设备"},
    {"MSG_NO_SIGNAL", "无视频信号"},
    {"MSG_SAVED", "保存成功"},
};

static struct {
    const translation_entry_t * entries;
    int count;
    char current_lang[16];
    bool initialized;
} g_lang = {0};

bool lang_manager_init(const char * lang_dir)
{
    (void)lang_dir;
    if(g_lang.initialized) return true;
    
    /* 默认使用中文 */
    g_lang.entries = zh_translations;
    g_lang.count = sizeof(zh_translations) / sizeof(zh_translations[0]);
    strncpy(g_lang.current_lang, "zh_CN", sizeof(g_lang.current_lang) - 1);
    
    g_lang.initialized = true;
    printf("[LangManager] Initialized (embedded), default: zh_CN\n");
    return true;
}

bool lang_load(const char * lang_code)
{
    if(!g_lang.initialized || !lang_code) return false;
    
    if(strncmp(lang_code, "zh", 2) == 0) {
        /* 中文 */
        g_lang.entries = zh_translations;
        g_lang.count = sizeof(zh_translations) / sizeof(zh_translations[0]);
    } else {
        /* 默认英文 */
        g_lang.entries = en_translations;
        g_lang.count = sizeof(en_translations) / sizeof(en_translations[0]);
    }
    
    strncpy(g_lang.current_lang, lang_code, sizeof(g_lang.current_lang) - 1);
    printf("[LangManager] Language loaded: %s\n", lang_code);
    return true;
}

const char * lang_get_current(void)
{
    return g_lang.current_lang;
}

const char * lang_get_text(const char * key)
{
    if(!key || !g_lang.initialized) return "";
    
    for(int i = 0; i < g_lang.count; i++) {
        if(strcmp(g_lang.entries[i].key, key) == 0) {
            return g_lang.entries[i].text;
        }
    }
    
    return key;
}

int lang_get_supported_list(lang_info_t * list, int max_count)
{
    if(!g_lang.initialized || !list || max_count < 2) return 0;
    
    /* 只支持中英文 */
    strncpy(list[0].code, "en_US", LANG_MAX_CODE_LEN - 1);
    strncpy(list[0].name, "English", LANG_MAX_NAME_LEN - 1);
    
    strncpy(list[1].code, "zh_CN", LANG_MAX_CODE_LEN - 1);
    strncpy(list[1].name, "简体中文", LANG_MAX_NAME_LEN - 1);
    
    return 2;
}

#include "config_manager.h"

bool lang_save_config(const char * lang_code)
{
    if(!lang_code) return false;

    config_t cfg;
    config_load(&cfg);
    strncpy(cfg.language, lang_code, sizeof(cfg.language) - 1);
    return config_save(&cfg);
}

bool lang_load_config(char * lang_code, int max_len)
{
    if(!lang_code || max_len <= 0) return false;

    config_t cfg;
    if(!config_load(&cfg)) return false;

    strncpy(lang_code, cfg.language, max_len - 1);
    lang_code[max_len - 1] = '\0';
    return true;
}
