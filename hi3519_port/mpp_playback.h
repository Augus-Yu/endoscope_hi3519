#ifndef ENDOSCOPE_HI3519_MPP_PLAYBACK_H
#define ENDOSCOPE_HI3519_MPP_PLAYBACK_H

#include <stdbool.h>

int  playback_start(const char *filepath);
int  playback_stop(void);
int  playback_is_running(void);

/* 暂停/继续 (toggle) */
void playback_pause_toggle(void);
int  playback_is_paused(void);

/* 进度: 返回 0-1000 (千分比, 精度更高) */
int  playback_get_progress(void);
int  playback_get_total_frames(void);
int  playback_get_current_frame(void);

/* 跳转 (seek): 按千分比位置跳转到最近的关键帧 */
int  playback_seek(int permille);

/* 快进/快退: 按帧数步进 */
int  playback_step_forward(int frames);
int  playback_step_backward(int frames);

#endif
