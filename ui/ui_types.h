#ifndef UI_TYPES_H
#define UI_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ENDOSCOPE_SCREEN_SPLASH,
    ENDOSCOPE_SCREEN_MAIN,
    ENDOSCOPE_SCREEN_SETTINGS,
    ENDOSCOPE_SCREEN_PLAYBACK,
    ENDOSCOPE_SCREEN_IMAGE_SETTINGS,
    ENDOSCOPE_SCREEN_PLAYER,
} endoscope_screen_t;

typedef struct {
    char id[32];
    char name[64];
    char gender[8];
    char age[8];
    bool has_data;
} patient_info_t;

typedef struct {
    bool endoscope_connected;
    bool usb_connected;
    bool is_recording;
    bool is_capturing;
    uint32_t recording_time;
    uint8_t battery_level;
    uint8_t storage_percent;
    patient_info_t patient;
} endoscope_status_t;

#ifdef __cplusplus
}
#endif

#endif
