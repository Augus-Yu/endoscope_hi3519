// Input device driver interface for Hi3519AV100 using Linux evdev
// This header exposes a minimal LVGL input device init function.

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

int lv_port_indev_init(void);
void lv_port_indev_deinit(void);

#endif // LV_PORT_INDEV_H
