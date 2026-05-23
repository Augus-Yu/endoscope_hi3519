/**
 * @file mpp_fpn.h
 * @brief FPN (Fixed Pattern Noise) 校准和校正
 */

#ifndef MPP_FPN_H
#define MPP_FPN_H

#include "sample_comm.h"

/* FPN 状态 */
typedef enum {
    FPN_STATUS_IDLE = 0,
    FPN_STATUS_OK,          /* 校准成功, 已启用校正 */
    FPN_STATUS_NEED_RETRY,  /* 暗帧不合格, 需手动遮光重试 */
    FPN_STATUS_SKIPPED,     /* 用户跳过 */
    FPN_STATUS_FAILED,      /* 校准失败 */
} fpn_status_t;

/* 开机自动FPN校准 (vi_init之后调用) */
fpn_status_t mpp_fpn_calibrate(VI_PIPE vi_pipe);

/* 获取当前FPN状态 */
fpn_status_t mpp_fpn_get_status(void);

/* 手动重试FPN校准 (用户在对话框中确认遮光后调用) */
fpn_status_t mpp_fpn_retry(VI_PIPE vi_pipe);

#endif /* MPP_FPN_H */
