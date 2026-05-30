#ifndef ENDOSCOPE_PLAYER_H
#define ENDOSCOPE_PLAYER_H

#include "lvgl.h"

void endoscope_player_init(void);
void endoscope_player_show(void);
void endoscope_player_hide(void);

/**
 * @brief 显示图片 (JPEG 等 LVGL 支持格式)
 *        需在调用 show() 之前设置
 * @param path 图片文件完整路径
 */
void endoscope_player_show_image(const char *path);

#endif
