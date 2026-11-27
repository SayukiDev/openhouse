# Valve Index HMD radio ("gravidongle")

The radio module inside the Index HMD: nRF52840 + **S140 6.1.1**, updated over USB
with Nordic's secure serial DFU. SteamVR ships its firmware as
`drivers/indexhmd/resources/firmware/radio/gd_*_dfu.zip` (an nrfutil package:
`manifest.json` + `temp_app_stamped.dat`/`.bin`, plus SoftDevice and bootloader
entries). Doorman runs on it as a single application image at `0x26000`.

```
make -C doorman/dongle-fw idx      -> build/doorman_dongle_idx.bin
```

## Flash layout

| region | address |
|---|---|
| MBR | `0x00000000`–`0x00000FFF` |
| S140 6.1.1 | `0x00001000`–`0x00025FFF` (kept, not used by doorman) |
| application (this image) | `0x00026000`–`0x000DEFFF` |
| pairing table | `0x000DF000` (last page before the bootloader, `link.c`) |
| MBR params page == DFU settings backup | `0x000FE000` |
| DFU settings | `0x000FF000` |
| Valve serial-DFU bootloader | `0x000E0000`–`0x000FDFFF` (`BOOTLOADER_SIZE` 0x1E000) |

The application is linked by `ld/app_idx.ld` (flash `0x26000`, RAM
`0x20000000 + 0x3E000`). Build flags: `-DVARIANT_SD=1 -DBOARD_IDX=1`.

## Board (`BOARD_IDX`)

| item | value |
|---|---|
| board id (cmd 0x01 byte 57) | 4 |
| USB product string | `Doorman Valve Index Radio`, VID/PID `6767:0727` |
| DFU entry magic | `0x11`/`0x10` both set `GPREGRET = 0xB1` (Valve's `BOOTLOADER_DFU_START`) |
| banks / boot stub | none — one image, updated through the bootloader |
| pairing page | `0xDF000` |
| LED | not driven (pin unknown) |
| FEM/PA pins | not configured (none known; only the Holyiot board has them) |

No SoftDevice is enabled; it stays in flash untouched and the application owns
`0x26000` upwards. `CMD_DFU_SERIAL` (0x11) asks the bootloader for DFU, and the
quick-crash counter does the same after three crashes within 10 s of boot.

## Updating a stock radio to doorman

The stock radio runs `28DE:2102` (application, HID; some HMDs also expose a CDC
port). Its bootloader is `28DE:2104` (CDC, same USB serial, generated as
`%02X%08X-%c%c%c`, e.g. `CEE3A511F0-RYB`).

**Valve's own updater** (SteamVR `tools/lighthouse/bin/win32/nrfutil.exe`) is stock
pc-nrfutil plus `nordicsemi/trigger_dfu.py`: it finds the HID device
`VID 0x28DE, PID 0x2102` by serial and sends feature report `0xFF` with
`FF 90 04 7a 6f 74 21` ("reboot into ISP"). The application then sets
`GPREGRET |= 0xB1` and resets, and nrfutil continues with `dfu usb_serial`
(hence `-cd 1`, one second for the re-enumeration):

```
nrfutil dfu usb_serial -cd 1 -pkg <package.zip> -p <serial>
```

So any nrfutil package with our `.dat` **and** the device version works through
Valve's tool; stock pc-nrfutil alone cannot trigger DFU.

**`radiotool`** (in `rw-index/delivery/radiotool.zip`, Windows + Linux) does the
whole chain and is the reference implementation:

```
radiotool list                      # radios (28DE:2102 / 28DE:2104), SteamVR package
radiotool info   -s SERIAL          # app report version, or bootloader stored version
radiotool dfu    -s SERIAL          # app -> DFU (the HID request above)
radiotool exit   -s SERIAL          # DFU -> app (DFU abort opcode 0x0C)
radiotool flash  doorman_dongle_idx.bin -s SERIAL [-y]
radiotool restore                    # stock application out of the SteamVR package
```

`flash` / `restore` take the signed init packet from the SteamVR radio package,
rebuild it with the firmware's size/hash and **the version the bootloader already
holds**, trigger DFU and flash. `-v` overrides the version.

**SteamVR's firmware wizard** is the third route: drop a package into
`drivers/indexhmd/resources/firmware/radio/`, raise `DongleV2Version` in
`manifest.vrfirmware`, and let SteamVR flash it.

## Packages

* Init packet: dfu-cc `Packet`, signature over the `InitCommand` payload
  (ECDSA P-256/SHA-256, `r||s` little-endian), image hash = SHA-256
  byte-reversed, `hw_version 0x2102`, `type 0` (application), `sd_req 0xB6`
  (S140 6.1.1).
* Only the application is written; the SoftDevice and the bootloader are not.
  Application updates are **single-bank**: bank 0 is invalidated and the image is
  written over the running application, so a failed transfer leaves the radio in
  its bootloader (recoverable), not a brick.
* Version rule on this bootloader: declared equal to or newer than the stored
  `app_version` is accepted, lower is refused (`0x10`, ext error fw version too
  low). Doorman declares `0x5CE89CD6`; the stock builds are `0x5CE89CD4`
  (1558748372) and `0x5C9E90B5` (1553895605), so stock ⇄ doorman round trips work
  as long as every package keeps `0x5CE89CD6`.
* From a UF2 (SoftDevice build, app linked at `0x26000`):
  `rw-index/uf2-to-valve-dfu/uf2_to_valve_dfu.py firmware.uf2 -o out -v 0x5CE89CD6`.

## Notes

* The devious bit: this bootloader is nRF5 SDK **15.2.0**, whose init-command
  decoding lets a packet carry the update parameters in one `InitCommand` and the
  signature over another (fixed in 15.3.0). That is why a package can be built
  from the stock package's signature. Full details in `rw-index/radio_info.md`
  and `rw-index/DFU_PROTOCOL.md`.
* With doorman running, USB is `6767:0727` (FICR-based serial), so SteamVR no
  longer sees the stock watchman radio; controllers then come through the doorman
  link.
* Restoring stock needs the stock application image: the SteamVR radio package
  for an Index. For another headset keep its own firmware before flashing.
* Other headsets with the same modules ("Valve VR Radio", e.g. Bigscreen Beyond,
  Pimax Dream Air): two radios per HMD, serials `-RYB` (Bluetooth side, may expose
  a CDC port) and `-LYX`; radio firmware versions are Valve Unix timestamps
  (`1525468496` = 2018-05-04 on a Dream Air). Use `radiotool list` / `info` /
  `dfu` to check whether that product is reachable the same way; if its bootloader
  uses a different key, pass its own package (`-P`) or `.dat` (`-r`).
* If a radio disappears from USB after a DFU attempt (Windows: "Device Descriptor
  Request Failed", Code 43), power-cycle the headset — a USB replug may not drop
  the radio's power. The bootloader also resets itself after its inactivity
  timeout.

## Installer

`openhouse-installer`: Install or Update Firmware > Doorman > Dongle > **Valve Index** (and Update
Dongles for one already running Doorman). It does what `radiotool flash` does, in
`src/common/nordic_dfu.cpp` (`FindValveBootloaders`, `ValveBootloaderInfo`, `ValveInitPacket`,
`ReadZipEntry`) and `apps/openhouse-installer/radio.cpp` (`FlashIndexRadio`):

* the reference `.dat` from SteamVR's newest `drivers/indexhmd/resources/firmware/radio/gd_*_dfu.zip`;
* one radio only: the `-RYB` radio (Bluetooth, base station power) is kept, the other is flashed;
  none is offered while one already runs Doorman;
* refuses (and exits the bootloader) unless the bootloader's application starts at `0x26000` and the
  reference is hardware `0x2102`; the version is the one the bootloader holds;
* the image is `firmwares/dongle/doorman-index.bin` (`build/doorman_dongle_idx.bin`).
