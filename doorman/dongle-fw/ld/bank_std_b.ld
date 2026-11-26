/* Firmware image for bank B, layout "std" (src/banks.h; MBR only: Nordic DFU boards (Holyiot, PCA10059, Kuroshin) and the Pro Micro "nosd" build):
   after the bank's 4 KB header page. Started by the boot stub. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00071000, LENGTH = 0x000D0000 - 0x00071000
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x3E000  /* 0x2003E000+: crash record, see main.c */
}
INCLUDE "nrf_common.ld"
/* nrf_common.ld takes __bss_start before .bss is aligned: when .data ends 4 bytes short of .bss's
   8-byte alignment, its __bss_end stops 4 bytes before the end of .bss and the last variable (newlib's
   __atexit) is never cleared: a HardFault before main() on whatever the bootloader left there. */
__bss_end = ADDR(.bss) + SIZEOF(.bss);
__bss_end__ = __bss_end;
