/**
 * @file ui_helpers.h
 * @brief UI辅助函数 - 提取重复代码模式
 */

#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_theme.h"
#include "font_manager.h"

/*********************
 *      DEFINES
 *********************/

/**
 * @brief 设置对象字体为当前语言字体
 * @param obj LVGL对象
 */
#define UI_SET_FONT(obj) do { \
    lv_font_t * _font = font_manager_get_font(); \
    if(_font) lv_obj_set_style_text_font(obj, _font, 0); \
} while(0)

/**********************
 * GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建水平分隔线
 * @param parent 父对象
 * @param width 线条宽度
 * @param y_pos Y轴位置
 * @return 线条对象
 */
lv_obj_t * ui_create_separator_line(lv_obj_t * parent, lv_coord_t width, lv_coord_t y_pos);

/**
 * @brief 格式化日期时间为字符串
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param prefix 前缀（可为NULL）
 * @param spaced_format 是否使用带空格格式（YYYY - MM - DD）
 */
void ui_format_datetime(char * buf, size_t buf_size, const char * prefix, bool spaced_format);

/**
 * @brief 更新标签的日期时间显示
 * @param label 标签对象
 * @param prefix 前缀文本
 */
void ui_update_label_datetime(lv_obj_t * label, const char * prefix);

/**
 * @brief 更新文本区域的日期时间显示
 * @param textarea 文本区域对象
 */
void ui_update_textarea_datetime(lv_obj_t * textarea);

#ifdef __cplusplus
}
#endif

#endif /* UI_HELPERS_H */
