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

/* 开机加载已有FPN文件并启用校正 (无文件则自动校准) */
fpn_status_t mpp_fpn_load(VI_PIPE vi_pipe);

/* 仅加载已有FPN文件, 不自动校准 (管线复用时使用) */
fpn_status_t mpp_fpn_load_file_only(VI_PIPE vi_pipe);

/* 手动重试FPN校准 (用户在对话框中确认遮光后调用) */
fpn_status_t mpp_fpn_retry(VI_PIPE vi_pipe);

/* 测试黑帧: 关LED+传感器曝光+ISP最小曝光, 不恢复 */
void mpp_fpn_test_dark_frame(VI_PIPE vi_pipe);

#endif /* MPP_FPN_H */
