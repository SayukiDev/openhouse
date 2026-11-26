// lhsession: the openhouse driver, built in, run without SteamVR until Ctrl+C. It works as it does in
// SteamVR (devices, room, settings, its telemetry), so openhouse-overlay, the dongle manager and the
// other tools can be used against it. Nothing is displayed and no app gets poses.
//
//   lhsession [--power-off]
//     --power-off  power controllers and trackers off on exit, as SteamVR does (off here: a quick
//                  restart would turn them all off)
#include "headless_host.hpp"

#include <csignal>

#ifdef _WIN32
#include <tlhelp32.h>
#endif

extern "C" void* HmdDriverFactory(const char* interfaceName, int* returnCode);   // the driver's entry

namespace {

std::atomic<bool> g_stop{false};
void OnSignal(int) { g_stop = true; }

// SteamVR (or Monado) with the driver loaded already has the devices.
bool RuntimeRunning() {
#ifdef _WIN32
  bool found = false;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W e{};
  e.dwSize = sizeof e;
  for (BOOL ok = Process32FirstW(snap, &e); ok && !found; ok = Process32NextW(snap, &e))
    found = !_wcsicmp(e.szExeFile, L"vrserver.exe");
  CloseHandle(snap);
  return found;
#else
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator("/proc", ec)) {
    std::FILE* f = std::fopen((e.path() / "comm").c_str(), "r");
    if (!f) continue;
    char comm[64]{};
    const bool got = std::fgets(comm, sizeof comm, f) != nullptr;
    std::fclose(f);
    if (!got) continue;
    const std::string name(comm, strcspn(comm, "\n"));
    if (name == "vrserver" || name == "monado-service" || name == "wivrn-server") return true;
  }
  return false;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  bool powerOff = false;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--power-off")) powerOff = true;
    else {
      std::fprintf(stderr, "usage: lhsession [--power-off]\n");
      return 2;
    }
  }
  if (RuntimeRunning()) {
    std::fprintf(stderr, "lhsession: SteamVR or Monado is running and has the devices; close it first\n");
    return 1;
  }
  if (!powerOff) {
#ifdef _WIN32
    _putenv_s("LH_NO_POWER_OFF_ON_EXIT", "1");
#else
    setenv("LH_NO_POWER_OFF_ON_EXIT", "1", 1);
#endif
  }
  int err = 0;
  auto* provider = static_cast<IServerTrackedDeviceProvider*>(HmdDriverFactory(IServerTrackedDeviceProvider_Version, &err));
  if (!provider) { std::fprintf(stderr, "lhsession: no driver (%d)\n", err); return 1; }

  g.printLog = true;
  g.discovering = true;  // as SteamVR: devices are taken whenever they turn up
  Host host;
  std::printf("lhsession: running (Ctrl+C stops)\n");
  std::fflush(stdout);
  if (provider->Init(&host) != VRInitError_None) { std::fprintf(stderr, "lhsession: the driver didn't start\n"); return 1; }
  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);
  while (!g_stop) {
    provider->RunFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(11));  // SteamVR's ~90 Hz
  }
  std::printf("lhsession: stopping\n");
  std::fflush(stdout);
  provider->Cleanup();
  return 0;
}
