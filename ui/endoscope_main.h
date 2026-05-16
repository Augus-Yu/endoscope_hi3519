/**
 * @file endoscope_main.h
 * @brief 主界面
 */

#ifndef ENDOSCOPE_MAIN_H
#define ENDOSCOPE_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void endoscope_main_init(void);
void endoscope_main_show(void);
void endoscope_main_hide(void);
void endoscope_main_update(void);
lv_obj_t * endoscope_main_get_screen(void);
void endoscope_main_reset_zoom(void);

#ifdef __cplusplus
}
#endif

#endif
