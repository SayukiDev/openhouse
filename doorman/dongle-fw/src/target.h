#pragma once
/* Physical memory bounds for the dongle firmware. Bank placement depends on the bootloader layout. */

#if defined(NRF52840_XXAA)
#define CHIP_FLASH_SIZE 0x00100000u
#define CHIP_RAM_SIZE   0x00040000u
#elif defined(NRF52833_XXAA)
#define CHIP_FLASH_SIZE 0x00080000u
#define CHIP_RAM_SIZE   0x00020000u
#else
#error "build with -DNRF52840_XXAA (1 MB / 256 KB) or -DNRF52833_XXAA (512 KB / 128 KB)"
#endif

#define RAM_BASE        0x20000000u
#define CHIP_FLASH_END  CHIP_FLASH_SIZE
#define CHIP_RAM_END    (RAM_BASE + CHIP_RAM_SIZE)

/* The linker leaves the top 8 KiB outside application RAM so the crash record survives a reset. */
#define CRASH_ADDR      (CHIP_RAM_END - 0x2000u)
