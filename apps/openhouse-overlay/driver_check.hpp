// The session check: a SteamVR update replaced openhouse's driver (while the installer's
// driver_lighthouse.*.stock backup is there: after uninstalling, SteamVR's driver is wanted and
// this stays quiet), or a newer openhouse release is out. Run by the overlay when SteamVR starts
// it (desktop dialog + dashboard banner + headset notification), and alone with --check (Linux:
// the Monado / WiVRn service; the dialog only). On Linux every run is logged to
// ~/.local/share/openhouse/check.log.
//
// Update (after a confirmation that VR shuts down) opens the release's page for its changelog and
// starts openhouse-installer --update, which downloads and installs it. Fix starts the installer
// openhouse keeps from the last install with --repair. Ignore hides it until the next session.
#pragma once
#include <string>

namespace openhouse::overlay {

enum class NoticeKind { None, Replaced, Newer, Firmware };
// Update: confirmed (Fix for a replaced driver). Uninstall: a replaced driver's other way out.
enum class NoticeAction { Update, Uninstall, Ignore };

// What the check found, for the overlay's banner; shared by the dialog and the banner.
struct NoticeState {
  NoticeKind kind = NoticeKind::None;
  std::string tag;      // Newer: the release ("v1.0.5"); Firmware: what needs it, one per line
  bool handled = false; // updated or ignored: nothing to show any more
  bool updating = false;
  std::string error;    // the installer couldn't be started
  int generation = 0;   // counts notices: one more firmware notice is a new one to show
};
NoticeState Notice();
// The banner's text ("" when there's nothing to show).
std::string NoticeText();
// From the overlay or the dialog. Update: the release page and the installer.
void ActOnNotice(NoticeAction action);
// What the confirmation lists: the programs the installer closes.
std::string ClosingText(bool underSteamVR);
// A newer release found by the overlay's own check (desktop window): the same banner.
void OfferUpdate(const std::string& tag);

// Watches the running driver (its telemetry) for the session: each Doorman Dongle that turns up
// (at the start, plugged in or replugged) is checked for firmware, and its devices for radios, older
// than what the installed openhouse came with; old ones are shown (dialog; the overlay's card and
// headset notification too), again for every dongle plugged in later. Update starts the installer
// with --dongles (Windows). Under Monado / WiVRn it returns once the driver goes away.
void RunFirmwareCheck(bool underSteamVR);
// The check for DRIVER_DIR (SteamVR/drivers/lighthouse/bin/<platform>); blocks while its dialog is
// up. 0: nothing to say or an update offered, 1: the driver was replaced, 2: no folder.
// underSteamVR: run by the overlay SteamVR started (what the confirmation lists).
int RunDriverCheck(const std::string& folder, bool underSteamVR = false);
// argv[first..] as one path: SteamVR passes a manifest's arguments with their quotes and may split
// a path with spaces.
std::string JoinedPath(int argc, char** argv, int first);

}  // namespace openhouse::overlay
