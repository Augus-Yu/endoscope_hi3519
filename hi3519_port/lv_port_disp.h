/* LVGL display driver interface for HiFB-based Hi3519 G3 overlay
 * This header declares the minimal API required by the LVGL PC port
 * to initialize and deinitialize the HiFB-based display driver.
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

// Avoid including LVGL headers here to keep the header light for different build setups.

// Initialize the LVGL display driver backed by HiFB G3 overlay
// Registers the LVGL display driver with LVGL.
int lv_port_disp_init(void);

// Deinitialize the display driver and release resources
void lv_port_disp_deinit(void);

/**
 * @brief Snapshot the current framebuffer content
 * @param buf Output buffer
 * @param buf_size Buffer size in bytes
 * @param width Output: framebuffer width
 * @param height Output: framebuffer height
 * @param stride Output: line stride in bytes
 * @param bpp Output: bits per pixel
 * @return 0 on success, -1 on failure
 */
int lv_port_disp_snapshot(void *buf, size_t buf_size, uint32_t *width, uint32_t *height, uint32_t *stride, int *bpp);

#endif // LV_PORT_DISP_H
