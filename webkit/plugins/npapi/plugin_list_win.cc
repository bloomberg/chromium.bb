// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_list.h"

#include <tchar.h>

#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/win/metro.h"
#include "base/win/pe_image.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/plugins/plugin_switches.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit {
namespace npapi {

namespace {

const char16 kRegistryApps[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const char16 kRegistryFirefox[] = L"firefox.exe";
const char16 kRegistryAcrobat[] = L"Acrobat.exe";
const char16 kRegistryAcrobatReader[] = L"AcroRd32.exe";
const char16 kRegistryWindowsMedia[] = L"wmplayer.exe";
const char16 kRegistryQuickTime[] = L"QuickTimePlayer.exe";
const char16 kRegistryPath[] = L"Path";
const char16 kRegistryFirefoxInstalled[] =
    L"SOFTWARE\\Mozilla\\Mozilla Firefox";
const char16 kRegistryJava[] =
    L"Software\\JavaSoft\\Java Runtime Environment";
const char16 kRegistryBrowserJavaVersion[] = L"BrowserJavaVersion";
const char16 kRegistryCurrentJavaVersion[] = L"CurrentVersion";
const char16 kRegistryJavaHome[] = L"JavaHome";
const char16 kJavaDeploy1[] = L"npdeploytk.dll";
const char16 kJavaDeploy2[] = L"npdeployjava1.dll";

// Gets the directory where the application data and libraries exist.  This
// may be a versioned subdirectory, or it may be the same directory as the
// GetExeDirectory(), depending on the embedder's implementation.
// Path is an output parameter to receive the path.
void GetAppDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath app_path;
  if (!PathService::Get(base::DIR_MODULE, &app_path))
    return;

  app_path = app_path.AppendASCII("plugins");
  plugin_dirs->insert(app_path);
}

// Gets the directory where the launching executable resides on disk.
// Path is an output parameter to receive the path.
void GetExeDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return;

  exe_path = exe_path.AppendASCII("plugins");
  plugin_dirs->insert(exe_path);
}

// Gets the installed path for a registered app.
bool GetInstalledPath(const char16* app, FilePath* out) {
  std::wstring reg_path(kRegistryApps);
  reg_path.append(L"\\");
  reg_path.append(app);

  base::win::RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
  std::wstring path;
  if (key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS) {
    *out = FilePath(path);
    return true;
  }

  return false;
}

// Search the registry at the given path and detect plugin directories.
void GetPluginsInRegistryDirectory(
    HKEY root_key,
    const std::wstring& registry_folder,
    std::set<FilePath>* plugin_dirs) {
  for (base::win::RegistryKeyIterator iter(root_key, registry_folder.c_str());
       iter.Valid(); ++iter) {
    // Use the registry to gather plugin across the file system.
    std::wstring reg_path = registry_folder;
    reg_path.append(L"\\");
    reg_path.append(iter.Name());
    base::win::RegKey key(root_key, reg_path.c_str(), KEY_READ);

    std::wstring path;
    if (key.ReadValue(kRegistryPath, &path) == ERROR_SUCCESS)
      plugin_dirs->insert(FilePath(path));
  }
}

// Enumerate through the registry key to find all installed FireFox paths.
// FireFox 3 beta and version 2 can coexist. See bug: 1025003
void GetFirefoxInstalledPaths(std::vector<FilePath>* out) {
  base::win::RegistryKeyIterator it(HKEY_LOCAL_MACHINE,
                                    kRegistryFirefoxInstalled);
  for (; it.Valid(); ++it) {
    std::wstring full_path = std::wstring(kRegistryFirefoxInstalled) + L"\\" +
                             it.Name() + L"\\Main";
    base::win::RegKey key(HKEY_LOCAL_MACHINE, full_path.c_str(), KEY_READ);
    std::wstring install_dir;
    if (key.ReadValue(L"Install Directory", &install_dir) != ERROR_SUCCESS)
      continue;
    out->push_back(FilePath(install_dir));
  }
}

// Get plugin directory locations from the Firefox install path.  This is kind
// of a kludge, but it helps us locate the flash player for users that
// already have it for firefox.  Not having to download yet-another-plugin
// is a good thing.
void GetFirefoxDirectory(std::set<FilePath>* plugin_dirs) {
  std::vector<FilePath> paths;
  GetFirefoxInstalledPaths(&paths);
  for (unsigned int i = 0; i < paths.size(); ++i) {
    plugin_dirs->insert(paths[i].Append(L"plugins"));
  }

  FilePath firefox_app_data_plugin_path;
  if (PathService::Get(base::DIR_APP_DATA, &firefox_app_data_plugin_path)) {
    firefox_app_data_plugin_path =
        firefox_app_data_plugin_path.AppendASCII("Mozilla")
                                    .AppendASCII("plugins");
    plugin_dirs->insert(firefox_app_data_plugin_path);
  }
}

// Hardcoded logic to detect Acrobat plugins locations.
void GetAcrobatDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (!GetInstalledPath(kRegistryAcrobatReader, &path) &&
      !GetInstalledPath(kRegistryAcrobat, &path)) {
    return;
  }

  plugin_dirs->insert(path.Append(L"Browser"));
}

// Hardcoded logic to detect QuickTime plugin location.
void GetQuicktimeDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (GetInstalledPath(kRegistryQuickTime, &path))
    plugin_dirs->insert(path.Append(L"plugins"));
}

// Hardcoded logic to detect Windows Media Player plugin location.
void GetWindowsMediaDirectory(std::set<FilePath>* plugin_dirs) {
  FilePath path;
  if (GetInstalledPath(kRegistryWindowsMedia, &path))
    plugin_dirs->insert(path);
}

// Hardcoded logic to detect Java plugin location.
void GetJavaDirectory(std::set<FilePath>* plugin_dirs) {
  // Load the new NPAPI Java plugin
  // 1. Open the main JRE key under HKLM
  base::win::RegKey java_key(HKEY_LOCAL_MACHINE, kRegistryJava,
                             KEY_QUERY_VALUE);

  // 2. Read the current Java version
  std::wstring java_version;
  if (java_key.ReadValue(kRegistryBrowserJavaVersion, &java_version) !=
      ERROR_SUCCESS) {
    java_key.ReadValue(kRegistryCurrentJavaVersion, &java_version);
  }

  if (!java_version.empty()) {
    java_key.OpenKey(java_version.c_str(), KEY_QUERY_VALUE);

    // 3. Install path of the JRE binaries is specified in "JavaHome"
    //    value under the Java version key.
    std::wstring java_plugin_directory;
    if (java_key.ReadValue(kRegistryJavaHome, &java_plugin_directory) ==
        ERROR_SUCCESS) {
      // 4. The new plugin resides under the 'bin/new_plugin'
      //    subdirectory.
      DCHECK(!java_plugin_directory.empty());
      java_plugin_directory.append(L"\\bin\\new_plugin");

      // 5. We don't know the exact name of the DLL but it's in the form
      //    NP*.dll so just invoke LoadPlugins on this path.
      plugin_dirs->insert(FilePath(java_plugin_directory));
    }
  }
}

bool IsValid32BitImage(const FilePath& path) {
  file_util::MemoryMappedFile plugin_image;

  if (!plugin_image.InitializeAsImageSection(path))
    return false;

  base::win::PEImage image(plugin_image.data());

  PIMAGE_NT_HEADERS nt_headers = image.GetNTHeaders();
  return (nt_headers->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
}

}  // anonymous namespace

void PluginList::PlatformInit() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  dont_load_new_wmp_ = command_line.HasSwitch(switches::kUseOldWMPPlugin);
}

void PluginList::GetPluginDirectories(std::vector<FilePath>* plugin_dirs) {
  // We use a set for uniqueness, which we require, over order, which we do not.
  std::set<FilePath> dirs;

  // Load from the application-specific area
  GetAppDirectory(&dirs);

  // Load from the executable area
  GetExeDirectory(&dirs);

  // Load Java
  GetJavaDirectory(&dirs);

  // Load firefox plugins too.  This is mainly to try to locate
  // a pre-installed Flash player.
  GetFirefoxDirectory(&dirs);

  // Firefox hard-codes the paths of some popular plugins to ensure that
  // the plugins are found.  We are going to copy this as well.
  GetAcrobatDirectory(&dirs);
  GetQuicktimeDirectory(&dirs);
  GetWindowsMediaDirectory(&dirs);

  for (std::set<FilePath>::iterator i = dirs.begin(); i != dirs.end(); ++i)
    plugin_dirs->push_back(*i);
}

void PluginList::GetPluginsInDir(
    const FilePath& path, std::vector<FilePath>* plugins) {
  WIN32_FIND_DATA find_file_data;
  HANDLE find_handle;

  std::wstring dir = path.value();
  // FindFirstFile requires that you specify a wildcard for directories.
  dir.append(L"\\NP*.DLL");

  find_handle = FindFirstFile(dir.c_str(), &find_file_data);
  if (find_handle == INVALID_HANDLE_VALUE)
    return;

  do {
    if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      FilePath filename = path.Append(find_file_data.cFileName);
      plugins->push_back(filename);
    }
  } while (FindNextFile(find_handle, &find_file_data) != 0);

  DCHECK(GetLastError() == ERROR_NO_MORE_FILES);
  FindClose(find_handle);
}

void PluginList::GetPluginPathsFromRegistry(std::vector<FilePath>* plugins) {
  std::set<FilePath> plugin_dirs;

  GetPluginsInRegistryDirectory(
      HKEY_CURRENT_USER, kRegistryMozillaPlugins, &plugin_dirs);
  GetPluginsInRegistryDirectory(
      HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins, &plugin_dirs);

  for (std::set<FilePath>::iterator i = plugin_dirs.begin();
       i != plugin_dirs.end(); ++i) {
    plugins->push_back(*i);
  }
}

// Returns true if the given plugins share at least one mime type.  This is used
// to differentiate newer versions of a plugin vs two plugins which happen to
// have the same filename.
bool HaveSharedMimeType(const webkit::WebPluginInfo& plugin1,
                        const webkit::WebPluginInfo& plugin2) {
  for (size_t i = 0; i < plugin1.mime_types.size(); ++i) {
    for (size_t j = 0; j < plugin2.mime_types.size(); ++j) {
      if (plugin1.mime_types[i].mime_type == plugin2.mime_types[j].mime_type)
        return true;
    }
  }

  return false;
}

// Compares Windows style version strings (i.e. 1,2,3,4).  Returns true if b's
// version is newer than a's, or false if it's equal or older.
bool IsNewerVersion(const std::wstring& a, const std::wstring& b) {
  std::vector<std::wstring> a_ver, b_ver;
  base::SplitString(a, ',', &a_ver);
  base::SplitString(b, ',', &b_ver);
  if (a_ver.size() == 1 && b_ver.size() == 1) {
    a_ver.clear();
    b_ver.clear();
    base::SplitString(a, '.', &a_ver);
    base::SplitString(b, '.', &b_ver);
  }
  if (a_ver.size() != b_ver.size())
    return false;
  for (size_t i = 0; i < a_ver.size(); i++) {
    int cur_a, cur_b;
    base::StringToInt(a_ver[i], &cur_a);
    base::StringToInt(b_ver[i], &cur_b);

    if (cur_a > cur_b)
      return false;
    if (cur_a < cur_b)
      return true;
  }
  return false;
}

bool PluginList::ShouldLoadPlugin(const webkit::WebPluginInfo& info,
                                  ScopedVector<PluginGroup>* plugin_groups) {
  // Version check

  for (size_t i = 0; i < plugin_groups->size(); ++i) {
    const std::vector<webkit::WebPluginInfo>& plugins =
        (*plugin_groups)[i]->web_plugin_infos();
    for (size_t j = 0; j < plugins.size(); ++j) {
      std::wstring plugin1 =
          StringToLowerASCII(plugins[j].path.BaseName().value());
      std::wstring plugin2 =
          StringToLowerASCII(info.path.BaseName().value());
      if ((plugin1 == plugin2 && HaveSharedMimeType(plugins[j], info)) ||
          (plugin1 == kJavaDeploy1 && plugin2 == kJavaDeploy2) ||
          (plugin1 == kJavaDeploy2 && plugin2 == kJavaDeploy1)) {
        if (!IsNewerVersion(plugins[j].version, info.version))
          return false;  // We have loaded a plugin whose version is newer.
        (*plugin_groups)[i]->RemovePlugin(plugins[j].path);
        break;
      }
    }
  }

  // Troublemakers

  std::wstring filename = StringToLowerASCII(info.path.BaseName().value());
  // Depends on XPCOM.
  if (filename == kMozillaActiveXPlugin)
    return false;

  // Disable the Yahoo Application State plugin as it crashes the plugin
  // process on return from NPObjectStub::OnInvoke. Please refer to
  // http://b/issue?id=1372124 for more information.
  if (filename == kYahooApplicationStatePlugin)
    return false;

  // Disable the WangWang protocol handler plugin (npww.dll) as it crashes
  // chrome during shutdown. Firefox also disables this plugin.
  // Please refer to http://code.google.com/p/chromium/issues/detail?id=3953
  // for more information.
  if (filename == kWanWangProtocolHandlerPlugin)
    return false;

  // We only work with newer versions of the Java plugin which use NPAPI only
  // and don't depend on XPCOM.
  if (filename == kJavaPlugin1 || filename == kJavaPlugin2) {
    std::vector<std::wstring> ver;
    base::SplitString(info.version, '.', &ver);
    int major, minor, update;
    if (ver.size() == 4 &&
        base::StringToInt(ver[0], &major) &&
        base::StringToInt(ver[1], &minor) &&
        base::StringToInt(ver[2], &update)) {
      if (major == 6 && minor == 0 && update < 120)
        return false;  // Java SE6 Update 11 or older.
    }
  }

  if (base::win::GetMetroModule()) {
    // In metro mode we only allow internal (pepper) plugins except flash.
    // TODO(cpu):remove this hack at some point in the future.
    if (info.type == WebPluginInfo::PLUGIN_TYPE_NPAPI)
      return false;
    if (filename == L"pepflashplayer.dll")
      return false;
  }

  // Special WMP handling

  // If both the new and old WMP plugins exist, only load the new one.
  if (filename == kNewWMPPlugin) {
    if (dont_load_new_wmp_)
      return false;

    for (size_t i = 0; i < plugin_groups->size(); ++i) {
      const std::vector<webkit::WebPluginInfo>& plugins =
          (*plugin_groups)[i]->web_plugin_infos();
      for (size_t j = 0; j < plugins.size(); ++j) {
        if (plugins[j].path.BaseName().value() == kOldWMPPlugin) {
          (*plugin_groups)[i]->RemovePlugin(plugins[j].path);
          break;
        }
      }
    }
  } else if (filename == kOldWMPPlugin) {
    for (size_t i = 0; i < plugin_groups->size(); ++i) {
      const std::vector<webkit::WebPluginInfo>& plugins =
          (*plugin_groups)[i]->web_plugin_infos();
      for (size_t j = 0; j < plugins.size(); ++j) {
        if (plugins[j].path.BaseName().value() == kNewWMPPlugin)
          return false;
      }
    }
  }

  HMODULE plugin_dll = NULL;
  bool load_plugin = true;

  // The plugin list could contain a 64 bit plugin which we cannot load.
  for (size_t i = 0; i < internal_plugins_.size(); ++i) {
    if (info.path == internal_plugins_[i].info.path)
      continue;

    if (file_util::PathExists(info.path) && (!IsValid32BitImage(info.path)))
      load_plugin = false;
    break;
  }
  return load_plugin;
}

}  // namespace npapi
}  // namespace webkit
