/**
 * @file sensor_config.h
 * @brief 多传感器配置抽象 - 编译时选择传感器类型
 */

#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include "sample_comm.h"

typedef enum {
    SENSOR_OV6946 = 0,
    SENSOR_OV9734,
} sensor_type_t;

typedef struct {
    sensor_type_t       type;
    SAMPLE_SNS_TYPE_E   sns_type;       /* SDK枚举, 如 OV_OV6946_DC_1M_30FPS */
    HI_S32              sns_id;         /* astViInfo[] 数组索引 */
    HI_S32              sns_bus_id;     /* I2C 总线 ID */

    /* 原生分辨率 */
    HI_U32              width;
    HI_U32              height;
    HI_U32              fps;

    /* VB 池参数 */
    PIC_SIZE_E          pic_size;
    HI_U32              vb_pool0_blk_cnt;
    HI_U32              vb_pool1_blk_cnt;
    HI_U32              vb_zoom_blk_cnt;

    /* 变焦上限 */
    HI_U32              zoom_max_w;
    HI_U32              zoom_max_h;

    /* 变焦级别表: [w0,h0, w1,h1, ...], 共 zoom_level_count 对 */
    HI_U32              zoom_level_count;
    const HI_U32       *zoom_levels;
} sensor_config_t;

extern const sensor_config_t g_sensor_config_ov6946;
extern const sensor_config_t g_sensor_config_ov9734;

const sensor_config_t *sensor_config_get_active(void);

/* 计算1x视频居中显示位置 */
static inline void sensor_config_get_display_rect(
    const sensor_config_t *cfg, HI_S32 *x, HI_S32 *y, HI_S32 *w, HI_S32 *h)
{
    *w = (HI_S32)cfg->width;
    *h = (HI_S32)cfg->height;
    *x = (1920 - *w) / 2;
    *y = (1080 - *h) / 2;
}

#endif /* SENSOR_CONFIG_H */
