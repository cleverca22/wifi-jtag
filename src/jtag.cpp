#include "jtag.h"
#include "pico/stdlib.h"
#include <stdio.h>

int tms, tdi, tck, tdo, trst;
static uint32_t gpio_old;
static uint64_t tdo_history;

typedef enum {
  unknown         = 0,
  testLogicReset  = 1,
  runTestIdle     = 2,
  selectDRScan    = 3,
  selectIRScan    = 4,
  captureDR       = 5,
  captureIR       = 6,
  shiftDR         = 7,
  shiftIR         = 8,
  exit1DR         = 9,
  exit1IR         = 10,
  pauseDR         = 11,
  pauseIR         = 12,
  exit2DR         = 13,
  exit2IR         = 14,
  updateDR        = 15,
  updateIR        = 16
} jtag_state;

static const char *mode_names[] = {
  [unknown] = "unknown",
  [testLogicReset] = "Test-Logic-Reset",
  [runTestIdle] = "Run-Test/Idle",
  [selectDRScan] = "Select DR-Scan",
  [selectIRScan] = "Select IR-Scan",
  [captureDR] = "Capture-DR",
  [captureIR] = "Capture-IR",
  [shiftDR] = "Shift-DR",
  [shiftIR] = "Shift-IR",
  [exit1DR] = "Exit1-DR",
  [exit1IR] = "Exit1-IR",
  [pauseDR] = "Pause-DR",
  [pauseIR] = "Pause-IR",
  [exit2DR] = "Exit2-DR",
  [exit2IR] = "Exit2-IR",
  [updateDR] = "Update-DR",
  [updateIR] = "Update-IR",
};

static jtag_state currentState;

void jtag_setup(int _tms, int _tdi, int _tck, int _tdo, int _trst) {
  tms = _tms;
  tdi = _tdi;
  tck = _tck;
  tdo = _tdo;
  trst = _trst;

  currentState = unknown;
  //int pin = 2;
  //for (int pin=9; pin<=12; pin++) {
    //gpio_set_function(pin, GPIO_FUNC_SIO);
    //gpio_set_dir(pin, GPIO_IN);
    //gpio_set_pulls(pin, true, false);
    //gpio_put(pin, 1);
    //printf("pin %d\n", pin);
  //}

  gpio_set_dir(tms, GPIO_OUT);
  gpio_set_dir(tdi, GPIO_OUT);
  gpio_set_dir(tck, GPIO_OUT);
  gpio_set_dir(trst, GPIO_OUT);
  gpio_set_dir(tdo, GPIO_IN);

  gpio_put(tck, 0);
  gpio_put(tms, 0);
  gpio_put(tdi, 0);
  gpio_put(trst, 0);

  gpio_set_function(tms, GPIO_FUNC_SIO);
  gpio_set_function(tdi, GPIO_FUNC_SIO);
  gpio_set_function(tck, GPIO_FUNC_SIO);
  gpio_set_function(tdo, GPIO_FUNC_SIO);
  gpio_set_function(trst, GPIO_FUNC_SIO);

  sleep_ms(10);
  gpio_put(trst, 1);

  gpio_old = gpio_get_all();
}

static void tck_pulse(void) {
  gpio_put(tck, 1);
  jtag_report();
  sleep_ms(1);
  gpio_put(tck, 0);
  jtag_report();
  sleep_ms(1);
}

void tck_shift(int flags) {
  if (flags & TMS) gpio_put(tms, 1);
  else gpio_put(tms, 0);

  if (flags & TDI) gpio_put(tdi, 1);
  else gpio_put(tdi, 0);

  busy_wait_us(1); // let TDI/TMS settle
  gpio_put(tck, 1); // shift reg and state machine steps on rising edge
  busy_wait_us(1); // wait for TDO to settle
  gpio_put(tck, 0);

  tdo_history = (tdo_history >> 1);
  if (gpio_get(tdo)) tdo_history |= 1ULL<<63;
}

uint64_t get_top_n_bits(int bits) {
  return tdo_history >> (64 - bits);
}

static uint64_t shift_data(uint64_t input, int bits) {
  uint64_t result = 0;
  for (int i=0; i<bits; i++) {
    int flag = 0;
    if (input & 1) flag |= TDI;

    if (i == (bits-1)) {
      flag |= TMS; // move to Exit1
      result = get_top_n_bits(bits);
    }

    tck_shift(flag);

    input = input >> 1;
  }
  //printf("result from shift: 0x%llx\n", result);
  return result;
}

void force_test_reset(void) {
  for (int i=0; i<5; i++) {
    tck_shift(TMS);
  }
  currentState = testLogicReset;
}

static void setJtagState(jtag_state newstate) {
  //printf("switching from %s to %s\n", mode_names[currentState], mode_names[newstate]);
  if ((currentState == testLogicReset) && (newstate == shiftIR)) {
    tck_shift(0);
    tck_shift(TMS);
    tck_shift(TMS);
    tck_shift(0);
    tck_shift(0);
    currentState = shiftIR;
  } else if ((currentState == exit1IR) && (newstate == shiftDR)) {
    tck_shift(TMS);
    tck_shift(TMS);
    tck_shift(0);
    tck_shift(0);
    currentState = shiftDR;
  } else if ((currentState == exit1IR) && (newstate == updateIR)) {
    tck_shift(TMS);
    currentState = updateIR;
  } else if ((currentState == updateIR) && (newstate == shiftDR)) {
    tck_shift(TMS);
    tck_shift(0);
    tck_shift(0);
    currentState = shiftDR;
  } else if ((currentState == exit1DR) && (newstate == shiftIR)) {
    tck_shift(TMS);
    tck_shift(TMS);
    tck_shift(TMS);
    tck_shift(0);
    tck_shift(0);
    currentState = shiftIR;
  } else if ((currentState == testLogicReset) && (newstate == shiftDR)) {
    tck_shift(0);
    tck_shift(TMS);
    tck_shift(0);
    tck_shift(0);
    currentState = shiftDR;
  } else {
    printf("cant switch from %s to %s\n", mode_names[currentState], mode_names[newstate]);
  }
}

uint64_t shift_ir(uint64_t cmd, int bits) {
  setJtagState(shiftIR);
  uint64_t result = shift_data(cmd, bits);
  currentState = exit1IR;
  return result;
}

uint64_t shift_dr(uint64_t data, int bits) {
  setJtagState(shiftDR);
  uint64_t result = shift_data(data, bits);
  currentState = exit1DR;
  return result;
}

void update_ir(void) {
  setJtagState(updateIR);
}

void jtag_test(void) {
  uint64_t out = 0;

  force_test_reset(); // now in Test-Logic-Reset
  puts("now in test-logic-reset");

  out = shift_ir(0b1110, 4);

  printf("IR returned %ld\n", (uint32_t)out);

  update_ir();

  out = shift_dr(0, 32);
  printf("IDCODE: 0x%lx\n", (uint32_t)out);

  shift_ir(0b1110, 4);
  update_ir();
  out = shift_dr(0, 32);
  printf("IDCODE: 0x%lx\n", (uint32_t)out);

  puts("test complete");
}

static void report_change(uint32_t old, uint32_t newval, int pin, const char *name) {
  int oldbit = (old & 1<<pin) != 0;
  int newbit = (newval & 1<<pin) != 0;
  if ((oldbit == 0) && (newbit == 1)) {
    printf("%s RISING\n", name);
  } else if ((oldbit == 1) && (newbit == 0)) {
    printf("%s FALLING\n", name);
  }
}

void jtag_report(void) {
  uint32_t gpio_new = gpio_get_all();
  report_change(gpio_old, gpio_new, tms, "TMS");
  report_change(gpio_old, gpio_new, tdi, "TDI");
  report_change(gpio_old, gpio_new, tck, "TCK");
  report_change(gpio_old, gpio_new, tdo, "TDO");
  report_change(gpio_old, gpio_new, 19, "RUN");

  gpio_old = gpio_new;
}
