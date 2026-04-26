/**
 * @file ui_helpers.c
 * @brief UI辅助函数实现
 */

#include <stdio.h>
#include <time.h>
#include "ui_helpers.h"
#include "lang_manager.h"

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * ui_create_separator_line(lv_obj_t * parent, lv_coord_t width, lv_coord_t y_pos)
{
    lv_obj_t * line = lv_line_create(parent);
    static lv_point_precise_t pts[2] = {{0, 0}, {0, 0}};
    pts[1].x = width;
    lv_line_set_points(line, pts, 2);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, y_pos);
    lv_obj_set_style_line_color(line, UI_COLOR_SEPARATOR, 0);
    return line;
}

void ui_format_datetime(char * buf, size_t buf_size, const char * prefix, bool spaced_format)
{
    time_t now = time(NULL);
    struct tm local_time_buf;
    struct tm * local_time = localtime_r(&now, &local_time_buf);

    if(prefix) {
        if(spaced_format) {
            snprintf(buf, buf_size, "%s %04d - %02d - %02d    %02d : %02d : %02d",
                     prefix,
                     local_time->tm_year + 1900,
                     local_time->tm_mon + 1,
                     local_time->tm_mday,
                     local_time->tm_hour,
                     local_time->tm_min,
                     local_time->tm_sec);
        }
        else {
            snprintf(buf, buf_size, "%s %04d-%02d-%02d  %02d:%02d:%02d",
                     prefix,
                     local_time->tm_year + 1900,
                     local_time->tm_mon + 1,
                     local_time->tm_mday,
                     local_time->tm_hour,
                     local_time->tm_min,
                     local_time->tm_sec);
        }
    }
    else {
        if(spaced_format) {
            snprintf(buf, buf_size, "%04d - %02d - %02d    %02d : %02d : %02d",
                     local_time->tm_year + 1900,
                     local_time->tm_mon + 1,
                     local_time->tm_mday,
                     local_time->tm_hour,
                     local_time->tm_min,
                     local_time->tm_sec);
        }
        else {
            snprintf(buf, buf_size, "%04d-%02d-%02d  %02d:%02d:%02d",
                     local_time->tm_year + 1900,
                     local_time->tm_mon + 1,
                     local_time->tm_mday,
                     local_time->tm_hour,
                     local_time->tm_min,
                     local_time->tm_sec);
        }
    }
}

void ui_update_label_datetime(lv_obj_t * label, const char * prefix)
{
    char buf[64];
    ui_format_datetime(buf, sizeof(buf), prefix, false);
    lv_label_set_text(label, buf);
}

void ui_update_textarea_datetime(lv_obj_t * textarea)
{
    char buf[64];
    ui_format_datetime(buf, sizeof(buf), NULL, true);
    lv_textarea_set_text(textarea, buf);
}
