/* Firmware image for bank B, layout "sd" (src/banks.h; Pro Micro "sd" build: the SoftDevice area (0x1000-0x25FFF) is left alone):
   after the bank's 4 KB header page. Started by the boot stub. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x0008F000, LENGTH = 0x000EC000 - 0x0008F000
  RAM   (rwx) : ORIGIN = 0x20003400, LENGTH = 0x3E000 - 0x3400  /* below: MBR/SoftDevice RAM (untouched). 0x2003E000+: crash record */
}
INCLUDE "nrf_common.ld"
/* nrf_common.ld takes __bss_start before .bss is aligned: when .data ends 4 bytes short of .bss's
   8-byte alignment, its __bss_end stops 4 bytes before the end of .bss and the last variable (newlib's
   __atexit) is never cleared: a HardFault before main() on whatever the bootloader left there. */
__bss_end = ADDR(.bss) + SIZEOF(.bss);
__bss_end__ = __bss_end;
