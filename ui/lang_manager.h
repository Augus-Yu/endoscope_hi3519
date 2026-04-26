/**
 * @file lang_manager.h
 * @brief 多语言管理器
 */

#ifndef LANG_MANAGER_H
#define LANG_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

#define LANG_MAX_KEY_LEN        64
#define LANG_MAX_TEXT_LEN       256
#define LANG_MAX_CODE_LEN       16
#define LANG_MAX_NAME_LEN       64
#define LANG_MAX_FONT_NAME_LEN  32

/* 语言结构体 */
typedef struct {
    char code[LANG_MAX_CODE_LEN];       /* 语言代码: zh_CN, en_US */
    char name[LANG_MAX_NAME_LEN];       /* 语言名称: 简体中文 */
} lang_info_t;

/* 初始化多语言系统 */
bool lang_manager_init(const char * lang_dir);

/* 加载指定语言 */
bool lang_load(const char * lang_code);

/* 获取当前语言代码 */
const char * lang_get_current(void);

/* 获取翻译文本 (核心函数) */
const char * lang_get_text(const char * key);

/* 获取支持的语言列表 */
int lang_get_supported_list(lang_info_t * list, int max_count);

/* 保存当前语言配置到文件 */
bool lang_save_config(const char * lang_code);

/* 从文件读取上次保存的语言配置 */
bool lang_load_config(char * lang_code, int max_len);

/* 简写宏 */
#define _TR(key) lang_get_text(key)
#define _FONT lang_get_font()

#ifdef __cplusplus
}
#endif

#endif
