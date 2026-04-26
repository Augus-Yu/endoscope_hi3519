/**
 * @file endoscope_playback.h
 * @brief 回放菜单
 */

#ifndef ENDOSCOPE_PLAYBACK_H
#define ENDOSCOPE_PLAYBACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void endoscope_playback_init(void);
void endoscope_playback_show(void);
void endoscope_playback_hide(void);

#ifdef __cplusplus
}
#endif

#endif
