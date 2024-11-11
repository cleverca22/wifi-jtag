#include <arm.h>
#include "jtag.h"
#include <stdio.h>
#include "pico/stdlib.h"

static void fail(const char *msg) {
  printf("failure: %s\n", msg);
  while (true) { sleep_ms(1000); };
}

uint32_t shift_dpacc(uint64_t data) {
  uint64_t out = shift_ir(0b1010, 4);
  if (out != 1) fail("IR fail");

  out = shift_dr(data, 35);
  if ((out & 7) != 2) {
    printf("DPACC = 0x%lx 0x%lx\n", (uint32_t)(out >> 3), (uint32_t)(out & 7));
    fail("");
  }
  return out >> 3;
}

uint32_t shift_apacc(uint64_t data) {
  uint64_t out = shift_ir(0b1011, 4);
  if (out != 1) fail("IR fail");

  out = shift_dr(data, 35);
  if ((out & 7) != 2) {
    printf("APACC = 0x%lx 0x%lx\n", (uint32_t)(out >> 3), (uint32_t)(out & 7));
    fail("");
  }
  return out >> 3;
}

uint32_t select_write(uint8_t apsel, uint8_t apbanksel, uint8_t dpbanksel) {
  uint64_t word = ((uint64_t)(apsel)<<24) | ((uint64_t)(apbanksel)<<4) | ((uint64_t)(dpbanksel)<<0);
  return shift_dpacc( (word << 3) | (0b10 << 1));
}

uint32_t dpacc_read(uint8_t bank, uint8_t reg) {
  uint64_t out;
  out = select_write(0, 0, bank);
  //printf("SELECT(prev): 0x%llx\n", out);
  out = shift_dpacc( ((reg >> 1) & 0b110) | 1);
  //printf("CTRL/STAT(prev): 0x%llx\n", out);
  out = shift_dpacc( (0b11 << 1) | 1);
  //printf("RDBUFF(prev): 0x%llx\n", out);
  printf("dpacc_read(0x%x, 0x%x) -> 0x%lx\n", bank, reg, (uint32_t)out);
  return out;
}

uint32_t dpacc_write(uint8_t bank, uint8_t reg, uint32_t data) {
  uint32_t out;
  uint64_t t = data;
  printf("dpacc_write(0x%x, 0x%x, 0x%lx)\n", bank, reg, data);
  out = select_write(0, 0, bank);
  //printf("SELECT(prev): 0x%llx\n", out);
  out = shift_dpacc( (t << 3) | ((reg >> 1) & 0b110) | 0);
  //printf("CTRL/STAT(prev): 0x%llx\n", out);
  out = shift_dpacc( (0b11 << 1) | 1);
  //printf("RDBUFF(prev): 0x%llx\n", out);
  return out;
}

uint32_t apacc_read(uint8_t ap, uint8_t apbanksel, uint8_t reg) {
  uint32_t out;
  out = select_write(ap, apbanksel, 0);
  out = shift_apacc(((reg >> 1) & 0b110) | 1);
  out = shift_dpacc( (0b11 << 1) | 1);
  return out;
}

uint32_t apacc_write(uint8_t ap, uint8_t apbanksel, uint8_t reg, uint32_t data) {
  uint32_t out;
  out = select_write(ap, apbanksel, 0);
  out = shift_apacc( ((reg>>1)&0b110) | 0);
  out = shift_dpacc( (0b11 << 1) | 1);
  return out;
}

void ap_tar(uint32_t addr) {
  apacc_write(0, 0, 4, addr);
}

uint32_t ap_bd(int slot) {
  return apacc_read(0, 1, slot*4);
}

static void decode_ap_idr(uint32_t val) {
  printf("  TYPE 0x%lx\n", val & 0xf);
  printf("  VARIANT 0x%lx\n", (val>>4) & 0xf);
  printf("  CLASS 0x%lx\n", (val>>13) & 0xf);
  printf("  DESIGNER 0x%lx\n", (val>>17) & 0x7f);
  printf("  REVISION 0x%lx\n", (val>>28) & 0xf);
}

void arm_test() {
  uint64_t out = 0;

  force_test_reset();

  out = shift_ir(0b1110, 4);
  printf("IR returned %ld\n", (uint32_t)out);
  update_ir();

  out = shift_dr(0, 32);
  printf("IDCODE: 0x%lx\n", (uint32_t)out);

  out = dpacc_read(0, 4);
  printf("CTRL/STAT 0x%llx\n", out);

  if ((out & (1<<29)) == 0) { // CDBGPWRUPACK not set
    out = dpacc_write(0, 4, (1<<28) | (1<<5));
    //printf("CTRL/STAT 0x%llx\n", out);

    out = dpacc_read(0, 4);
    printf("CTRL/STAT 0x%llx\n", out);
  }

  if ((out & (1<<31)) == 0) { // CSYSPWRUPACK, not set
    out = dpacc_write(0, 4, (1<<30) | (1<<28) | (1<<5));
    //printf("CTRL/STAT 0x%llx\n", out);
  }

  out = dpacc_read(0x2, 0x4);
  printf("TARGETID: 0x%lx\n", (uint32_t)out);

  out = dpacc_read(0, 0);
  printf("DPIDR: 0x%llx\n", out);

  uint32_t ap_base = apacc_read(0, 0xf, 0x8);
  if (ap_base) {
    printf("AP(0) BASE: 0x%lx\n", ap_base);
  }

  apacc_write(0, 0, 8, 0); // TAR[63:32]

  for (uint32_t i = ap_base; i < (ap_base + 256); i += 16) {
    ap_tar(i);
    uint32_t a,b,c,d;
    a = ap_bd(0);
    b = ap_bd(1);
    c = ap_bd(2);
    d = ap_bd(3);
    printf("0x%lx: %08lx %08lx %08lx %08lx\n", i, a, b, c, d);
  }

  for (int i=0; i<16; i++) {
    out = apacc_read(i, 0, 0);
    if (out) {
      printf("AP(%d) CSW: 0x%llx\n", i, out);
    }

    out = apacc_read(i, 0, 4);
    if (out) {
      printf("AP(%d) TAR: 0x%llx\n", i, out);
    }

    out = apacc_read(i, 0, 0xC);
    if (out) {
      printf("AP(%d) DRW: 0x%llx\n", i, out);
    }

    for (int j=0; j<4; j++) {
      out = apacc_read(i, 0x1, j*4);
      if (out) {
        printf("AP(%d) BD%d: 0x%llx\n", i, j, out);
      }
    }

    out = apacc_read(i, 0xf, 0x4);
    if (out) {
      printf("AP(%d) CFG: 0x%llx\n", i, out);
    }
    out = apacc_read(i, 0xf, 0xc);
    if (out) {
      printf("AP(%d) IDR: 0x%llx\n", i, out);
      decode_ap_idr((uint32_t)out);
    }
    out = apacc_read(i, 0xf, 0x8);
    if (out) {
      printf("AP(%d) BASE: 0x%llx\n", i, out);
    }
  }

  uint32_t out32 = dpacc_read(0,4);
  printf("CTRL/STAT: 0x%lx\n", out32);

  puts("test complete");
}
