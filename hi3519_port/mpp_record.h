/*
 * Hi3519AV100 Endoscope - VENC recording and snapshot interface
 * Header for MPP-based recording and snapshot utilities
 *
 * Exposes a simple API to start/stop H.264 recording of the VI->VENC
 * stream and to save JPEG snapshots from the live video.
 */

#ifndef ENDOSCOPE_HI3519_MPP_RECORD_H
#define ENDOSCOPE_HI3519_MPP_RECORD_H

#include <stddef.h>

// Recording APIs
int record_init(void);
int record_start(const char* filename); // filename is ignored; generated internally
int record_stop(void);
int record_deinit(void);

// Snapshot API
int snapshot_save(const char* filename);

// Helper to generate filenames with a given extension (ext should include dot, e.g. ".h264" or ".jpg")
void generate_filename(char* buf, size_t size, const char* ext);

#endif // ENDOSCOPE_HI3519_MPP_RECORD_H
