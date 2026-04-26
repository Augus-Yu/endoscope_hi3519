/**
 * @file endoscope_splash.h
 * @brief 开机画面
 */

#ifndef ENDOSCOPE_SPLASH_H
#define ENDOSCOPE_SPLASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

/**********************
 * GLOBAL FUNCTIONS
 **********************/

void endoscope_splash_init(void);
void endoscope_splash_show(void);
void endoscope_splash_hide(void);

#ifdef __cplusplus
}
#endif

#endif
