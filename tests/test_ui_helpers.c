#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include "unity.h"

static void ui_format_datetime(char * buf, size_t buf_size, const char * prefix, bool spaced_format)
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

void setUp(void)
{
}

void tearDown(void)
{
}

void test_ui_format_datetime_no_prefix_compact(void)
{
    char buf[64];
    ui_format_datetime(buf, sizeof(buf), NULL, false);

    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_TRUE(strlen(buf) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "  "));
    TEST_ASSERT_NOT_NULL(strstr(buf, "-"));
    TEST_ASSERT_NOT_NULL(strstr(buf, ":"));
}

void test_ui_format_datetime_with_prefix_compact(void)
{
    char buf[64];
    ui_format_datetime(buf, sizeof(buf), "SNAPSHOT", false);

    TEST_ASSERT_EQUAL_STRING_LEN("SNAPSHOT", buf, strlen("SNAPSHOT"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "-"));
    TEST_ASSERT_NOT_NULL(strstr(buf, ":"));
}

void test_ui_format_datetime_no_prefix_spaced(void)
{
    char buf[64];
    ui_format_datetime(buf, sizeof(buf), NULL, true);

    TEST_ASSERT_NOT_NULL(strstr(buf, " - "));
    TEST_ASSERT_NOT_NULL(strstr(buf, " : "));
}

void test_ui_format_datetime_with_prefix_spaced(void)
{
    char buf[64];
    ui_format_datetime(buf, sizeof(buf), "REC", true);

    TEST_ASSERT_EQUAL_STRING_LEN("REC", buf, strlen("REC"));
    TEST_ASSERT_NOT_NULL(strstr(buf, " - "));
    TEST_ASSERT_NOT_NULL(strstr(buf, " : "));
}

void test_ui_format_datetime_buffer_truncation(void)
{
    char buf[10];
    ui_format_datetime(buf, sizeof(buf), "VERY_LONG_PREFIX_THAT_WILL_BE_TRUNCATED", false);

    buf[sizeof(buf) - 1] = '\0';
    TEST_ASSERT_TRUE(strlen(buf) < sizeof(buf));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_ui_format_datetime_no_prefix_compact);
    RUN_TEST(test_ui_format_datetime_with_prefix_compact);
    RUN_TEST(test_ui_format_datetime_no_prefix_spaced);
    RUN_TEST(test_ui_format_datetime_with_prefix_spaced);
    RUN_TEST(test_ui_format_datetime_buffer_truncation);

    return UNITY_END();
}
