#include "screen_manager.h"
#include "lvgl.h"

typedef struct {
    screen_init_fn_t init;
    screen_show_fn_t show;
    screen_hide_fn_t hide;
    bool initialized;
} screen_entry_t;

static screen_entry_t screens[6] = {0};
static int current_screen = -1;

void screen_manager_register(endoscope_screen_t id,
                             screen_init_fn_t init,
                             screen_show_fn_t show,
                             screen_hide_fn_t hide)
{
    if (id < 0 || id >= 5) return;
    screens[id].init = init;
    screens[id].show = show;
    screens[id].hide = hide;
    screens[id].initialized = false;
}

void screen_manager_navigate_to(endoscope_screen_t id)
{
    if (id < 0 || id >= 5) return;

    screen_entry_t *entry = &screens[id];
    if (!entry->init || !entry->show) return;

    if (current_screen >= 0 && current_screen < 5) {
        screen_entry_t *prev = &screens[current_screen];
        if (prev->hide) prev->hide();
    }

    if (!entry->initialized) {
        entry->init();
        entry->initialized = true;
    }

    entry->show();
    current_screen = id;
}

endoscope_screen_t screen_manager_get_current(void)
{
    return current_screen;
}

void screen_manager_init(void)
{
    for (int i = 0; i < 5; i++) {
        screens[i].initialized = false;
    }
    current_screen = ENDOSCOPE_SCREEN_SPLASH;
}

void screen_manager_invalidate(endoscope_screen_t id)
{
    if (id < 0 || id >= 5) return;
    screens[id].initialized = false;
}
