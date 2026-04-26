/**
 * @file endoscope_image_settings.h
 * @brief 图像设置
 */

#ifndef ENDOSCOPE_IMAGE_SETTINGS_H
#define ENDOSCOPE_IMAGE_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void endoscope_image_settings_init(void);
void endoscope_image_settings_show(void);
void endoscope_image_settings_hide(void);

#ifdef __cplusplus
}
#endif

#endif
