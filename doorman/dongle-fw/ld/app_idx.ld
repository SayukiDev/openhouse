/* Valve Index HMD radio ("gravidongle"), nRF52840:
     MBR                0x000000-0x000FFF
     S140 6.1.1         0x001000-0x025FFF  (kept, never written by an app update)
     app (this image)   0x026000-0x0DEFFF  (nrf_dfu_bank0_start_addr() = ALIGN(SD size))
     pairing table      0x0DF000           (last page before Valve's bootloader; link.c)
     bootloader         0x0E0000-0x0FFFFF  (Valve serial-DFU bootloader + settings, untouched)
   RAM: all 256 KB up to the crash record page at 0x2003E000 (main.c). */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00026000, LENGTH = 0x000DF000 - 0x00026000
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x0003E000
}
INCLUDE "nrf_common.ld"
