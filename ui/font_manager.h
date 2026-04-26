/**
 * @file font_manager.h
 * @brief TTF 字体管理器 - 支持完整字符输入
 * @brief TTF Font Manager - Support full character input
 */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* 字体配置 - 1920x1080分辨率下的字体大小 */
#define FONT_PATH_PREFIX        "./lang/fonts/"
#define FONT_DEFAULT_SIZE       24
#define FONT_LARGE_SIZE         48
#define FONT_SMALL_SIZE         16

/* 字体类型 */
typedef enum {
    FONT_TYPE_LATIN = 0,       /* 拉丁语系: 欧洲/南美/北美 */
    FONT_TYPE_CJK,             /* 中日韩 */
    FONT_TYPE_THAI,            /* 泰语 */
    FONT_TYPE_ARABIC,          /* 阿拉伯语 (RTL) */
    FONT_TYPE_MYANMAR,         /* 缅甸语 */
    FONT_TYPE_COUNT
} font_type_t;

/* 字体管理器 */
typedef struct {
    lv_font_t * fonts[FONT_TYPE_COUNT];        /* 当前加载的字体 */
    lv_font_t * fonts_large[FONT_TYPE_COUNT];  /* 大字体版本 */
    font_type_t current_type;                   /* 当前字体类型 */
    bool is_rtl;                               /* 是否 RTL 语言 */
} font_manager_t;

/**********************
 * GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化字体管理器
 * @param font_dir 字体文件目录 (如: "/opt/endoscope/fonts")
 * @return true 成功, false 失败
 */
bool font_manager_init(const char * font_dir);

/**
 * @brief 根据语言代码加载字体
 * @param lang_code 语言代码 (如: "zh_CN", "en_US")
 * @return true 成功, false 失败
 */
bool font_manager_load_for_language(const char * lang_code);

/**
 * @brief 获取当前字体
 * @return 当前语言的 lv_font_t 指针
 */
lv_font_t * font_manager_get_font(void);

/**
 * @brief 获取当前大字体
 * @return 当前语言的大号字体
 */
lv_font_t * font_manager_get_font_large(void);

/**
 * @brief 获取指定类型的字体
 * @param type 字体类型
 * @return 字体指针
 */
lv_font_t * font_manager_get_font_by_type(font_type_t type);

/**
 * @brief 检查当前是否是 RTL 语言
 * @return true 是 RTL, false 不是
 */
bool font_manager_is_rtl(void);

/**
 * @brief 获取当前字体类型
 * @return 字体类型
 */
font_type_t font_manager_get_current_type(void);

/**
 * @brief 重新加载字体（切换语言后调用）
 * @param lang_code 新的语言代码
 */
void font_manager_reload(const char * lang_code);

/**
 * @brief 释放所有字体资源
 */
void font_manager_deinit(void);

/**
 * @brief 根据语言代码判断字体类型
 * @param lang_code 语言代码
 * @return 字体类型
 */
font_type_t font_manager_detect_type(const char * lang_code);

/**
 * @brief 检查语言是否是 RTL
 * @param lang_code 语言代码
 * @return true 是 RTL
 */
bool font_manager_lang_is_rtl(const char * lang_code);

#ifdef __cplusplus
}
#endif

#endif /* FONT_MANAGER_H */
