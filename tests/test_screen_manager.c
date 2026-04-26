#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "unity.h"

#include "../ui/ui_types.h"

static int g_init_called = 0;
static int g_show_called = 0;
static int g_hide_called = 0;

typedef void (*screen_init_fn_t)(void);
typedef void (*screen_show_fn_t)(void);
typedef void (*screen_hide_fn_t)(void);

static void screen_manager_register(endoscope_screen_t id,
                                    screen_init_fn_t init,
                                    screen_show_fn_t show,
                                    screen_hide_fn_t hide);
static void screen_manager_navigate_to(endoscope_screen_t id);
static endoscope_screen_t screen_manager_get_current(void);
static void screen_manager_init(void);

typedef struct {
    screen_init_fn_t init;
    screen_show_fn_t show;
    screen_hide_fn_t hide;
    bool initialized;
} screen_entry_t;

static screen_entry_t screens[5] = {0};
static endoscope_screen_t current_screen = ENDOSCOPE_SCREEN_SPLASH;

static void screen_manager_register(endoscope_screen_t id,
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

static void screen_manager_navigate_to(endoscope_screen_t id)
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

static endoscope_screen_t screen_manager_get_current(void)
{
    return current_screen;
}

static void screen_manager_init(void)
{
    for (int i = 0; i < 5; i++) {
        screens[i].initialized = false;
    }
    current_screen = ENDOSCOPE_SCREEN_SPLASH;
}

static void mock_init(void)
{
    g_init_called++;
}

static void mock_show(void)
{
    g_show_called++;
}

static void mock_hide(void)
{
    g_hide_called++;
}

void setUp(void)
{
    g_init_called = 0;
    g_show_called = 0;
    g_hide_called = 0;
    screen_manager_init();
}

void tearDown(void)
{
}

void test_screen_manager_register_valid_screen(void)
{
    screen_manager_register(ENDOSCOPE_SCREEN_MAIN, mock_init, mock_show, mock_hide);

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);

    TEST_ASSERT_EQUAL(1, g_init_called);
    TEST_ASSERT_EQUAL(1, g_show_called);
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_MAIN, screen_manager_get_current());
}

void test_screen_manager_register_multiple_screens(void)
{
    screen_manager_register(ENDOSCOPE_SCREEN_MAIN, mock_init, mock_show, mock_hide);
    screen_manager_register(ENDOSCOPE_SCREEN_SETTINGS, mock_init, mock_show, mock_hide);

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_MAIN, screen_manager_get_current());

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_SETTINGS);
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_SETTINGS, screen_manager_get_current());
    TEST_ASSERT_EQUAL(2, g_init_called);
    TEST_ASSERT_EQUAL(2, g_show_called);
    TEST_ASSERT_EQUAL(1, g_hide_called);
}

void test_screen_manager_navigate_to_same_screen_no_reinit(void)
{
    screen_manager_register(ENDOSCOPE_SCREEN_MAIN, mock_init, mock_show, mock_hide);

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
    TEST_ASSERT_EQUAL(1, g_init_called);

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
    TEST_ASSERT_EQUAL(1, g_init_called);
    TEST_ASSERT_EQUAL(2, g_show_called);
}

void test_screen_manager_navigate_to_invalid_screen_ignored(void)
{
    screen_manager_navigate_to(99);
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_SPLASH, screen_manager_get_current());
    TEST_ASSERT_EQUAL(0, g_show_called);
}

void test_screen_manager_register_null_callbacks(void)
{
    screen_manager_register(ENDOSCOPE_SCREEN_MAIN, NULL, NULL, NULL);

    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);

    TEST_ASSERT_EQUAL(0, g_init_called);
    TEST_ASSERT_EQUAL(0, g_show_called);
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_SPLASH, screen_manager_get_current());
}

void test_screen_manager_init_resets_state(void)
{
    screen_manager_register(ENDOSCOPE_SCREEN_MAIN, mock_init, mock_show, mock_hide);
    screen_manager_navigate_to(ENDOSCOPE_SCREEN_MAIN);
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_MAIN, screen_manager_get_current());

    screen_manager_init();
    TEST_ASSERT_EQUAL(ENDOSCOPE_SCREEN_SPLASH, screen_manager_get_current());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_screen_manager_register_valid_screen);
    RUN_TEST(test_screen_manager_register_multiple_screens);
    RUN_TEST(test_screen_manager_navigate_to_same_screen_no_reinit);
    RUN_TEST(test_screen_manager_navigate_to_invalid_screen_ignored);
    RUN_TEST(test_screen_manager_register_null_callbacks);
    RUN_TEST(test_screen_manager_init_resets_state);

    return UNITY_END();
}
