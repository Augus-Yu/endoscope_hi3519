/**
 * @file endoscope_settings.h
 * @brief 设置菜单
 */

#ifndef ENDOSCOPE_SETTINGS_H
#define ENDOSCOPE_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void endoscope_settings_init(void);
void endoscope_settings_show(void);
void endoscope_settings_hide(void);

#ifdef __cplusplus
}
#endif

#endif
