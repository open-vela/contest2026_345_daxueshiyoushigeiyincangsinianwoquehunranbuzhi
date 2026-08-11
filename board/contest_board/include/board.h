/****************************************************************************
 * Contest 2026 team 345 ESP32-P4X Function EV Board definitions
 ****************************************************************************/

#ifndef __VENDOR_OPENVELA_BOARDS_CONTEST2026_345_INCLUDE_BOARD_H
#define __VENDOR_OPENVELA_BOARDS_CONTEST2026_345_INCLUDE_BOARD_H

/* J1 GPIO loopback and BOOT-button test pins for board V1.8.  Connect J1
 * pin 13 (GPIO20) to J1 pin 11 (GPIO21) when running the GPIO loopback
 * smoke test.  GPIO35 also routes through R135 to RMII_TXD1; do not expose
 * the BOOT-button GPIO interrupt when Ethernet is enabled.
 */

#define BOARD_GPIO_OUT      20
#define BOARD_GPIO_IN       21
#define BUTTON_BOOT         35

#define BOARD_NGPIOOUT       1
#define BOARD_NGPIOIN        1
#define BOARD_NGPIOINT       1

#endif
