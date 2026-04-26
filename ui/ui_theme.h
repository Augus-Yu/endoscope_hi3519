#ifndef UI_THEME_H
#define UI_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define UI_COLOR_BG             lv_color_hex(0x0a1628)
#define UI_COLOR_SURFACE        lv_color_hex(0x1a2744)
#define UI_COLOR_TEXT           lv_color_hex(0xffffff)
#define UI_COLOR_TEXT_SECONDARY lv_color_hex(0xB0B0B0)
#define UI_COLOR_ACCENT         lv_color_hex(0x2196F3)
#define UI_COLOR_PRIMARY        lv_color_hex(0x2196F3)
#define UI_COLOR_SECONDARY      lv_color_hex(0x1976D2)
#define UI_COLOR_SUCCESS        lv_color_hex(0x4CAF50)
#define UI_COLOR_WARNING        lv_color_hex(0xFF9800)
#define UI_COLOR_ERROR          lv_color_hex(0xF44336)
#define UI_COLOR_RECORDING      lv_color_hex(0xFF0000)

#define UI_COLOR_BUTTON_BG      lv_color_hex(0x333333)
#define UI_COLOR_BUTTON_CANCEL  lv_color_hex(0x5a6a7a)
#define UI_COLOR_INPUT_BG       lv_color_hex(0x1e2d4d)
#define UI_COLOR_DISABLED       lv_color_hex(0x666666)
#define UI_COLOR_SEPARATOR      lv_color_hex(0x333333)

#define UI_COLOR_COLORKEY       lv_color_hex(0x00FF00)

#ifdef __cplusplus
}
#endif

#endif
