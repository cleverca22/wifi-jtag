#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int tms, tdi, tck, tdo, trst;

void jtag_setup(int tms, int tdi, int tck, int tdo, int trst);
void jtag_test(void);
void jtag_report(void);
void force_test_reset(void);
uint64_t shift_ir(uint64_t cmd, int bits);
uint64_t shift_dr(uint64_t data, int bits);
void update_ir(void);
void tck_shift(int flags);

#define TMS 1
#define TDI 2

#ifdef __cplusplus
};
#endif

// rpi jtag pins
// TRST(22) is an input that the pico-pullup can pull high


// and set enable_jtag_gpio=1 in config.txt
// pico -> rpi
// 0        15  RX      in
// 1        14  TX      out
// 6        27  TMS
// 7        26  TDI     input
// 8        25  TCK     input
// 9        24  TDO     stuck high, out?
// 10       23  RTCK    stuck low, out?
// 11       22  TRST    input
// 19           RUN     in

// 2024-11-03 14:38:08 < PaulFertser> Cortex-M has 0x4BA00477 IDCODE
// 2024-11-03 14:39:08 < PaulFertser> Cortex-A7 0x6ba00477

// pi2 0x4ba00477
// pi3 0x4ba00477
