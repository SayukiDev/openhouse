// Headless OpenVR host for the drop-in driver, modelled on Monado's steamvr_lh:
// dlopen + HmdDriverFactory, Activate() called from inside TrackedDeviceAdded(),
// devices only accepted during a discovery window, properties/inputs recorded.
// Runs the driver on a replayed capture (LH_REPLAY) and checks what a host sees.
#include "../../apps/lhsession/headless_host.hpp"

namespace {

int failures = 0;
#define CHECK(cond, ...)                                       \
  do {                                                         \
    if (!(cond)) {                                             \
      ++failures;                                              \
      std::printf("FAIL %s: ", #cond);                         \
      std::printf(__VA_ARGS__);                                \
      std::printf("\n");                                       \
    }                                                          \
  } while (0)

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s driver_lighthouse.so capture.lhraw [seconds]\n", argv[0]);
    return 2;
  }
  const bool live = std::string(argv[2]) == "live";
  if (!live && !std::filesystem::exists(argv[2])) {
    std::printf("SKIP: capture %s not found\n", argv[2]);
    return 0;
  }
  if (!live) {
#ifdef _WIN32
    _putenv_s("LH_REPLAY", argv[2]);
#else
    setenv("LH_REPLAY", argv[2], 1);
#endif
  }
  // Keep the driver's room map, host frame and SteamVR files out of the real config
  // directory: a test run wrote its rooms into the user's hostframe.json.
  const auto scratch = std::filesystem::temp_directory_path() / ("lh_mock_host_" + std::to_string(
#ifdef _WIN32
      _getpid()
#else
      getpid()
#endif
      ));
  std::filesystem::create_directories(scratch);
  const auto setDefault = [](const char* name, const std::string& value) {
    if (std::getenv(name)) return;
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
  };
  setDefault("LH_ROOM_MAP", (scratch / "roommap.json").string());
  setDefault("LH_REFERENCE_DB", (scratch / "lighthousedb.json").string());
  const double seconds = argc > 3 ? std::atof(argv[3]) : 12;
  using Factory = void* (*)(const char*, int*);
#ifdef _WIN32
  HMODULE lib = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
  if (!lib) { std::printf("FAIL LoadLibrary: %lu\n", GetLastError()); return 1; }
  auto factory = reinterpret_cast<Factory>(GetProcAddress(lib, "HmdDriverFactory"));
#else
  void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!lib) { std::printf("FAIL dlopen: %s\n", dlerror()); return 1; }
  auto factory = reinterpret_cast<Factory>(dlsym(lib, "HmdDriverFactory"));
#endif
  if (!factory) { std::printf("FAIL: no HmdDriverFactory\n"); return 1; }
  int err = 0;
  auto* provider = static_cast<IServerTrackedDeviceProvider*>(factory(IServerTrackedDeviceProvider_Version, &err));
  if (!provider) { std::printf("FAIL: factory returned null (%d)\n", err); return 1; }
  Host host;
  g.initTime = std::chrono::steady_clock::now();
  if (provider->Init(&host) != VRInitError_None) { std::printf("FAIL: Init\n"); return 1; }
  // Discovery window like Monado's LH_DISCOVER_WAIT_MS (real time, replay runs in real time).
  const auto start = std::chrono::steady_clock::now();
  auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); };
  bool standbyTested = false;
  while (elapsed() < seconds) {
    if (!live && elapsed() > 8.0 && !standbyTested) {
      std::vector<ITrackedDeviceServerDriver*> devices;
      { std::lock_guard lock(g.mutex); for (const auto& d : g.devices) devices.push_back(d.driver); }
      for (auto* d : devices) d->EnterStandby();
      standbyTested = true;
    }
    if (elapsed() > 6.0) g.discovering = false;
    provider->RunFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(11));
  }
  provider->Cleanup();

  std::lock_guard lock(g.mutex);
  std::printf("devices accepted %zu, declined %d\n", g.devices.size(), g.declined);
  const Recorded::Dev* controller = nullptr;
  for (const auto& d : g.devices) {
    std::printf("  %-14s class %d profile %-55s role %d poses %d (valid %d) universe %llu battery %.2f\n",
                d.serial.c_str(), int(d.cls), PropString(d, Prop_InputProfilePath_String).c_str(),
                PropValue<int32_t>(d, Prop_ControllerRoleHint_Int32, -1), d.poses, d.valid,
                static_cast<unsigned long long>(PropValue<uint64_t>(d, Prop_CurrentUniverseId_Uint64, 0)),
                PropValue<float>(d, Prop_DeviceBatteryPercentage_Float, -1));
    CHECK(d.cls != TrackedDeviceClass_HMD, "HMDs must not be exposed");
    if (d.cls == TrackedDeviceClass_Controller) controller = &d;
  }
  CHECK(controller != nullptr, "the Index controller must be registered");
  if (controller) {
    CHECK(PropString(*controller, Prop_InputProfilePath_String) == "{indexcontroller}/input/index_controller_profile.json", "profile");
    CHECK(PropString(*controller, Prop_ControllerType_String) == "knuckles", "controller type");
    CHECK(PropValue<bool>(*controller, Prop_DeviceCanPowerOff_Bool, false), "power-off capability");
    CHECK(PropValue<bool>(*controller, Prop_Identifiable_Bool, false), "identify capability");
    CHECK(PropString(*controller, Prop_TrackingSystemName_String) == "lighthouse", "tracking system");
    CHECK(PropValue<int32_t>(*controller, Prop_ControllerRoleHint_Int32, 0) == TrackedControllerRole_RightHand, "right hand");
    CHECK(PropValue<uint64_t>(*controller, Prop_CurrentUniverseId_Uint64, 0) != 0, "universe id");
    CHECK(PropValue<float>(*controller, Prop_DeviceBatteryPercentage_Float, -1) > 0, "battery");
    for (const char* path : {"/input/system/click", "/input/a/click", "/input/a/touch", "/input/b/click", "/input/trigger/value",
                             "/input/trigger/click", "/input/grip/force", "/input/grip/value", "/input/thumbstick/x",
                             "/input/thumbstick/y", "/input/trackpad/force", "/input/finger/index", "/output/haptic"})
      CHECK(controller->inputs.count(path), "missing input %s", path);
    CHECK(controller->valid > 50, "valid poses: %d", controller->valid);
    const auto& p = controller->last;
    const double qn = std::sqrt(p.qRotation.w * p.qRotation.w + p.qRotation.x * p.qRotation.x + p.qRotation.y * p.qRotation.y + p.qRotation.z * p.qRotation.z);
    CHECK(std::fabs(qn - 1) < 1e-6, "unit quaternion");
    CHECK(p.poseTimeOffset <= 0 && p.poseTimeOffset > -0.1, "pose time offset %f", p.poseTimeOffset);
    CHECK(p.result == TrackingResult_Running_OK, "result %d", int(p.result));
    std::printf("  last pose (%.3f %.3f %.3f) offset %.1f ms, input updates: trigger %d, finger/index %d\n", p.vecPosition[0],
                p.vecPosition[1], p.vecPosition[2], p.poseTimeOffset * 1000, g.inputUpdates["/input/trigger/value"],
                g.inputUpdates["/input/finger/index"]);
  }
  CHECK(g.hapticEvents == 2, "haptic events were polled");
  CHECK(g.unroutedHaptics == 0, "SteamVR- and Monado-style haptic events reach their controller");
  int dropped = 0, held = 0;
  for (const auto& d : g.devices) { dropped += d.dropped; held += d.held; }
  CHECK(dropped == 0, "lost devices held at their last pose, not dropped to the origin: %d dropped", dropped);
  std::printf("  lost-and-held poses %d\n", held);
  if (!live && seconds > 9) CHECK(g.powerOffRequests > 0, "standby routed a power-off request");
  // Asset-dependent checks are opt-in so a source checkout without SteamVR still tests.
  if (std::getenv("LH_HAND_POSES")) {
    CHECK(g.skeletonCreates == 1, "skeleton created: %d", g.skeletonCreates);
    CHECK(g.skeletonUpdates > 10, "skeleton updates: %d", g.skeletonUpdates);
    CHECK(g.invalidBones == 0, "invalid bone data: %d", g.invalidBones);
  }
  std::printf("  skeletons %d, updates %d, invalid bones %d; haptic events %d\n", g.skeletonCreates, g.skeletonUpdates, g.invalidBones, g.hapticEvents);
  std::printf(failures ? "%d FAILURES\n" : "PASS\n", failures);
  return failures ? 1 : 0;
}
