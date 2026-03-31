#ifndef MY_APPLICATION_USB_H
#define MY_APPLICATION_USB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void My_Application_OnUsbReceived(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* MY_APPLICATION_USB_H */
