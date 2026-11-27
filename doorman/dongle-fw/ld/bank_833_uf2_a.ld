/* nRF52833 UF2 layout, no SoftDevice. See src/banks.h. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00011000, LENGTH = 0x00030000 - 0x00011000
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x1E000  /* 0x2001E000+: crash record, see src/target.h */
}
INCLUDE "nrf_common.ld"
