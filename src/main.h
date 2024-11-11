#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void cdc_has_data(int interface);
void led_toggle();
void led_set(int val);
extern bool led_idle_blink;
#ifdef __cplusplus
};
#endif
