#ifndef ENDOSCOPE_HI3519_MPP_PLAYBACK_H
#define ENDOSCOPE_HI3519_MPP_PLAYBACK_H

int playback_start(const char *filepath);
int playback_stop(void);
int playback_is_running(void);

#endif
