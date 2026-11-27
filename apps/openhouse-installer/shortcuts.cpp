#include "shortcuts.hpp"

#include <cstdlib>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace openhouse::installer {
namespace {

#ifdef _WIN32
fs::path Folder() {
  PWSTR p = nullptr;
  fs::path out;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &p))) out = fs::path(p) / L"openhouse";
  CoTaskMemFree(p);
  return out;
}

bool Write(const fs::path& folder, const Shortcut& s) {
  IShellLinkW* link = nullptr;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&link))))
    return false;
  link->SetPath(s.exe.wstring().c_str());
  link->SetWorkingDirectory(s.exe.parent_path().wstring().c_str());
  link->SetDescription(Path(s.comment).wstring().c_str());
  link->SetIconLocation(s.exe.wstring().c_str(), 0);
  IPersistFile* file = nullptr;
  bool ok = SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)));
  if (ok) {
    // Named by what the menu shows.
    ok = SUCCEEDED(file->Save((folder / (Path(s.name).wstring() + L".lnk")).wstring().c_str(), TRUE));
    file->Release();
  }
  link->Release();
  return ok;
}
#else
// Every id an install may have made: removed on uninstall, and when an install no longer has it.
const char* const kIds[] = {"openhouse", "doorman-dongle-manager", "update-check", "installer"};

fs::path Folder() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  return (xdg && *xdg ? Path(xdg) : HomeDirectory() / ".local" / "share") / "applications";
}
fs::path File(const fs::path& folder, const std::string& id) { return folder / ("openhouse-" + id + ".desktop"); }

// An Exec argument: quoted, with the characters the spec reserves inside quotes escaped (and the
// backslash doubled again for the key's own escapes).
std::string Arg(const std::string& a) {
  std::string out = "\"";
  for (char c : a) {
    if (c == '"' || c == '`' || c == '$' || c == '\\') out += "\\\\";
    if (c == '%') out += '%';
    out += c;
  }
  return out + "\"";
}

bool Write(const fs::path& folder, const Shortcut& s) {
  std::string d = "[Desktop Entry]\nType=Application\nName=" + s.name + "\nComment=" + s.comment +
                  "\nExec=" + Arg(Utf8(s.exe)) + "\nPath=" + Utf8(s.exe.parent_path()) +
                  "\nIcon=" + (s.icon.empty() ? std::string("input-gaming") : Utf8(s.icon)) + "\nTerminal=false\nCategories=Utility;\n";
  if (s.hidden) d += "NoDisplay=true\n";
  if (s.xrPlugin || !s.xrPluginArgs.empty()) {
    d += "X-XR-Plugin=true\nX-XR-Plugin-Exec=" + Arg(Utf8(s.exe));
    for (const auto& a : s.xrPluginArgs) d += " " + Arg(a);
    d += "\nX-XR-Plugin-Execution-Time=after-start\n";
  }
  const fs::path f = File(folder, s.id);
  if (!WriteFileAtomic(f, d)) return false;
  std::error_code ec;
  fs::permissions(f, fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read, ec);
  return true;
}
#endif

void RemoveAll(const fs::path& folder) {
#ifdef _WIN32
  std::error_code ec;
  fs::remove_all(folder, ec);  // only ours are in it
#else
  for (const char* id : kIds) RemoveFile(File(folder, id));
#endif
}

}  // namespace

std::string InstallShortcuts(const std::vector<Shortcut>& shortcuts) {
  const fs::path folder = Folder();
  if (folder.empty()) return "no menu folder";
  RemoveAll(folder);
  std::error_code ec;
  fs::create_directories(folder, ec);
#ifdef _WIN32
  const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif
  std::string failed;
  for (const auto& s : shortcuts)
    if (!Write(folder, s)) failed += (failed.empty() ? "" : ", ") + s.name;
#ifdef _WIN32
  if (SUCCEEDED(co)) CoUninitialize();
#endif
  return failed;
}

fs::path ShortcutFile(const std::string& id) {
#ifdef _WIN32
  (void)id;
  return {};
#else
  return File(Folder(), id);
#endif
}

void RemoveShortcuts() {
  const fs::path folder = Folder();
  if (!folder.empty()) RemoveAll(folder);
}

#ifdef _WIN32
namespace {
bool SetValue(HKEY key, const wchar_t* sub, const wchar_t* name, const std::wstring& value) {
  HKEY k;
  if (RegCreateKeyExW(key, sub, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
  const bool ok = RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                 DWORD((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
  RegCloseKey(k);
  return ok;
}
}  // namespace

bool InstallUrlHandler(const fs::path& installer) {
  const wchar_t* root = L"Software\\Classes\\openhouse";
  return SetValue(HKEY_CURRENT_USER, root, nullptr, L"URL:openhouse") &&
         SetValue(HKEY_CURRENT_USER, root, L"URL Protocol", L"") &&
         SetValue(HKEY_CURRENT_USER, L"Software\\Classes\\openhouse\\shell\\open\\command", nullptr,
                  L"\"" + installer.wstring() + L"\" --url \"%1\"");
}

void RemoveUrlHandler() { RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\openhouse"); }
#else
namespace {
const char* kUrlEntry = "openhouse-url.desktop";
// mimeapps.list's [Default Applications] with ours for x-scheme-handler/openhouse (add: true) or without it.
void SetDefault(bool add) {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  const fs::path file = (xdg && *xdg ? Path(xdg) : HomeDirectory() / ".config") / "mimeapps.list";
  std::ifstream in(file);
  std::string line, out, section;
  bool done = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] == '[') {
      if (add && section == "[Default Applications]" && !done) { out += std::string("x-scheme-handler/openhouse=") + kUrlEntry + "\n"; done = true; }
      section = line;
    } else if (line.rfind("x-scheme-handler/openhouse=", 0) == 0) {
      continue;
    }
    out += line + "\n";
  }
  if (add && !done) {
    if (section != "[Default Applications]") out += "[Default Applications]\n";
    out += std::string("x-scheme-handler/openhouse=") + kUrlEntry + "\n";
  }
  in.close();
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  WriteFileAtomic(file, out);
}
}  // namespace

bool InstallUrlHandler(const fs::path& installer) {
  const std::string d = "[Desktop Entry]\nType=Application\nName=openhouse\nExec=" + Arg(Utf8(installer)) +
                        " --url %u\nMimeType=x-scheme-handler/openhouse;\nNoDisplay=true\nTerminal=false\n";
  const fs::path folder = Folder();
  std::error_code ec;
  fs::create_directories(folder, ec);
  if (!WriteFileAtomic(folder / kUrlEntry, d)) return false;
  SetDefault(true);
  return true;
}

void RemoveUrlHandler() {
  RemoveFile(Folder() / kUrlEntry);
  SetDefault(false);
}
#endif

}  // namespace openhouse::installer
