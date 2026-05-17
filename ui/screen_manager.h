#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "endoscope_ui.h"

typedef void (*screen_init_fn_t)(void);
typedef void (*screen_show_fn_t)(void);
typedef void (*screen_hide_fn_t)(void);

void screen_manager_register(endoscope_screen_t id,
                             screen_init_fn_t init,
                             screen_show_fn_t show,
                             screen_hide_fn_t hide);

void screen_manager_navigate_to(endoscope_screen_t id);

endoscope_screen_t screen_manager_get_current(void);

void screen_manager_invalidate(endoscope_screen_t id);

void screen_manager_init(void);

#ifdef __cplusplus
}
#endif

#endif
