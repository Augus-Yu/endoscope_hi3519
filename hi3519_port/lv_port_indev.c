#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include "lvgl.h"
#include "lv_port_indev.h"

#define SCREEN_W     1920
#define SCREEN_H     1080
#define MOUSE_SENSITIVITY 2

extern lv_indev_t * lv_indev_create(void);
extern void lv_indev_set_type(lv_indev_t * indev, lv_indev_type_t type);
extern void lv_indev_set_read_cb(lv_indev_t * indev, void (*read_cb)(lv_indev_t * indev, lv_indev_data_t * data));
extern void lv_indev_set_user_data(lv_indev_t * indev, void * user_data);

static int mouse_fd = -1;
static lv_indev_t * g_indev = NULL;
static lv_obj_t * mouse_cursor = NULL;
static int32_t mouse_x = SCREEN_W / 2;
static int32_t mouse_y = SCREEN_H / 2;
static int32_t mouse_btn = 0;

static int is_mouse_device(const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return 0;

    unsigned long ev_bits = 0;
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), &ev_bits) < 0) {
        close(fd);
        return 0;
    }

    unsigned long rel_bits = 0;
    if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), &rel_bits) < 0) {
        close(fd);
        return 0;
    }

    int has_rel_x = (rel_bits & (1 << REL_X)) != 0;
    int has_rel_y = (rel_bits & (1 << REL_Y)) != 0;
    int has_keys = (ev_bits & (1 << EV_KEY)) != 0;

    close(fd);
    return has_rel_x && has_rel_y && has_keys;
}

static int find_mouse_device(char *out_path, size_t out_size)
{
    DIR *dir = opendir("/dev/input");
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        if (is_mouse_device(path)) {
            strncpy(out_path, path, out_size - 1);
            out_path[out_size - 1] = '\0';
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return -1;
}

static void create_mouse_cursor(void)
{
    if (mouse_cursor && lv_obj_is_valid(mouse_cursor)) {
        lv_obj_delete(mouse_cursor);
    }

    lv_obj_t * scr = lv_screen_active();
    if (!scr) return;

    mouse_cursor = lv_obj_create(lv_layer_top());
    lv_obj_set_size(mouse_cursor, 20, 20);
    lv_obj_set_style_bg_color(mouse_cursor, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(mouse_cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mouse_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(mouse_cursor, 2, 0);
    lv_obj_set_style_border_color(mouse_cursor, lv_color_black(), 0);
    lv_obj_clear_flag(mouse_cursor, LV_OBJ_FLAG_CLICKABLE);

    if (g_indev) {
        lv_indev_set_cursor(g_indev, mouse_cursor);
    }
}

static void mouse_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    (void)indev;

    if (!mouse_cursor || !lv_obj_is_valid(mouse_cursor)) {
        create_mouse_cursor();
    }

    if (mouse_fd < 0) {
        data->point.x = mouse_x;
        data->point.y = mouse_y;
        data->state = mouse_btn ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        return;
    }

    struct input_event ev;

    while (read(mouse_fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
            if (ev.code == REL_X) {
                mouse_x += ev.value * MOUSE_SENSITIVITY;
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
            } else if (ev.code == REL_Y) {
                mouse_y += ev.value * MOUSE_SENSITIVITY;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;
            }
        } else if (ev.type == EV_KEY) {
            if (ev.code == BTN_LEFT) {
                mouse_btn = ev.value;
            }
        }
    }

    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_btn ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int lv_port_indev_init(void)
{
    char mouse_path[256] = {0};
    if (find_mouse_device(mouse_path, sizeof(mouse_path)) == 0) {
        mouse_fd = open(mouse_path, O_RDONLY | O_NONBLOCK);
        if (mouse_fd >= 0) {
            printf("[LV_PORT_INDEV] Mouse device opened: %s\n", mouse_path);
        }
    }

    if (mouse_fd < 0) {
        fprintf(stderr, "[LV_PORT_INDEV] Warning: No mouse device found\n");
    }

    g_indev = lv_indev_create();
    if (g_indev == NULL) {
        fprintf(stderr, "[LV_PORT_INDEV] lv_indev_create failed\n");
        if (mouse_fd >= 0) {
            close(mouse_fd);
            mouse_fd = -1;
        }
        return -1;
    }

    lv_indev_set_type(g_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_indev, mouse_read_cb);
    lv_indev_set_user_data(g_indev, (void*)(intptr_t)mouse_fd);

    printf("[LV_PORT_INDEV] Input device initialized\n");
    return 0;
}

void lv_port_indev_deinit(void)
{
    if (mouse_cursor && lv_obj_is_valid(mouse_cursor)) {
        lv_obj_delete(mouse_cursor);
        mouse_cursor = NULL;
    }
    if (mouse_fd >= 0) {
        close(mouse_fd);
        mouse_fd = -1;
    }
    g_indev = NULL;
}
