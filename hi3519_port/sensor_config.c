/**
 * @file sensor_config.c
 * @brief 传感器配置表 + 编译时选择器
 */

#include "sensor_config.h"

/* OV6946: 400x400, 1.5x→600, 2x→800 */
static const HI_U32 g_ov6946_zoom_levels[] = {
    400, 400,
    600, 600,
    800, 800,
};

const sensor_config_t g_sensor_config_ov6946 = {
    .type             = SENSOR_OV6946,
    .sns_type         = OV_OV6946_DC_1M_30FPS,
    .sns_id           = 1,
    .sns_bus_id       = 2,
    .width            = 400,
    .height           = 400,
    .fps              = 30,
    .pic_size         = PIC_400P,
    .vb_pool0_blk_cnt = 20,
    .vb_pool1_blk_cnt = 15,
    .vb_zoom_blk_cnt  = 8,
    .zoom_max_w       = 800,
    .zoom_max_h       = 800,
    .zoom_level_count = 3,
    .zoom_levels      = g_ov6946_zoom_levels,
};

/* OV9734: 1280x720, 变焦保持16:9 */
static const HI_U32 g_ov9734_zoom_levels[] = {
    1280,  720,
    1760,  990,
    2560, 1440,
};

const sensor_config_t g_sensor_config_ov9734 = {
    .type             = SENSOR_OV9734,
    .sns_type         = OV_OV9734_MIPI_1M_30FPS,
    .sns_id           = 0,
    .sns_bus_id       = 4,
    .width            = 1280,
    .height           = 720,
    .fps              = 30,
    .pic_size         = PIC_720P,
    .vb_pool0_blk_cnt = 8,
    .vb_pool1_blk_cnt = 6,
    .vb_zoom_blk_cnt  = 4,
    .zoom_max_w       = 2560,
    .zoom_max_h       = 1440,
    .zoom_level_count = 3,
    .zoom_levels      = g_ov9734_zoom_levels,
};

const sensor_config_t *sensor_config_get_active(void)
{
#if defined(ACTIVE_SENSOR_OV9734)
    return &g_sensor_config_ov9734;
#else
    return &g_sensor_config_ov6946;
#endif
}
