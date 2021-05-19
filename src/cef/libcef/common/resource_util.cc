// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/resource_util.h"

#if defined(OS_LINUX)
#include <dlfcn.h>
#endif

#include "libcef/features/runtime.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/layout.h"

#if defined(OS_MAC)
#include "base/mac/foundation_util.h"
#include "libcef/common/util_mac.h"
#endif

#if defined(OS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

namespace resource_util {

namespace {

#if defined(OS_LINUX)

// Based on chrome/common/chrome_paths_linux.cc.
// See http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
// for a spec on where config files go.  The net effect for most
// systems is we use ~/.config/chromium/ for Chromium and
// ~/.config/google-chrome/ for official builds.
// (This also helps us sidestep issues with other apps grabbing ~/.chromium .)
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.Append(FILE_PATH_LITERAL("cef_user_data"));
  return true;
}

#elif defined(OS_MAC)

// Based on chrome/common/chrome_paths_mac.mm.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_APP_DATA, result))
    return false;
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#elif defined(OS_WIN)

// Based on chrome/common/chrome_paths_win.cc.
bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  *result = result->Append(FILE_PATH_LITERAL("CEF"));
  *result = result->Append(FILE_PATH_LITERAL("User Data"));
  return true;
}

#endif

base::FilePath GetUserDataPath(CefSettings* settings,
                               const base::CommandLine* command_line) {
  // |settings| will be non-nullptr in the main process only.
  if (settings) {
    // With the Chrome runtime Profile paths must always be relative to the
    // user data directory, so defaulting to |root_cache_path| first is
    // appropriate.
    CefString user_data_path;
    if (cef::IsChromeRuntimeEnabled() && settings->root_cache_path.length > 0) {
      user_data_path = CefString(&settings->root_cache_path);
    }
    if (user_data_path.empty() && settings->user_data_path.length > 0) {
      user_data_path = CefString(&settings->user_data_path);
    }
    if (!user_data_path.empty()) {
      return base::FilePath(user_data_path);
    }
  }

  // This may be set for sub-processes.
  base::FilePath result =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  if (!result.empty())
    return result;

  if (GetDefaultUserDataDirectory(&result))
    return result;

  if (base::PathService::Get(base::DIR_TEMP, &result))
    return result;

  NOTREACHED();
  return result;
}

// Consider downloads 'dangerous' if they go to the home directory on Linux and
// to the desktop on any platform.
// From chrome/browser/download/download_prefs.cc.
bool DownloadPathIsDangerous(const base::FilePath& download_path) {
#if defined(OS_LINUX)
  base::FilePath home_dir = base::GetHomeDir();
  if (download_path == home_dir) {
    return true;
  }
#endif

  base::FilePath desktop_dir;
  if (!base::PathService::Get(base::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
}

bool GetDefaultDownloadDirectory(base::FilePath* result) {
  // This will return the safe download directory if necessary.
  return chrome::GetUserDownloadsDirectory(result);
}

bool GetDefaultDownloadSafeDirectory(base::FilePath* result) {
  // Start with the default download directory.
  if (!GetDefaultDownloadDirectory(result))
    return false;

  if (DownloadPathIsDangerous(*result)) {
#if defined(OS_WIN) || defined(OS_LINUX)
    // Explicitly switch to the safe download directory.
    return chrome::GetUserDownloadsDirectorySafe(result);
#else
    // No viable alternative on macOS.
    return false;
#endif
  }

  return true;
}

}  // namespace

#if defined(OS_MAC)

base::FilePath GetResourcesDir() {
  return util_mac::GetFrameworkResourcesDirectory();
}

// Use a "~/Library/Logs/<app name>_debug.log" file where <app name> is the name
// of the running executable.
base::FilePath GetDefaultLogFilePath() {
  std::string exe_name = util_mac::GetMainProcessPath().BaseName().value();
  return base::mac::GetUserLibraryPath()
      .Append(FILE_PATH_LITERAL("Logs"))
      .Append(FILE_PATH_LITERAL(exe_name + "_debug.log"));
}

#else  // !defined(OS_MAC)

base::FilePath GetResourcesDir() {
  base::FilePath pak_dir;
  base::PathService::Get(base::DIR_ASSETS, &pak_dir);
  return pak_dir;
}

// Use a "debug.log" file in the running executable's directory.
base::FilePath GetDefaultLogFilePath() {
  base::FilePath log_path;
  base::PathService::Get(base::DIR_EXE, &log_path);
  return log_path.Append(FILE_PATH_LITERAL("debug.log"));
}

#endif  // !defined(OS_MAC)

void OverrideDefaultDownloadDir() {
  base::FilePath dir_default_download;
  base::FilePath dir_default_download_safe;
  if (GetDefaultDownloadDirectory(&dir_default_download)) {
    base::PathService::Override(chrome::DIR_DEFAULT_DOWNLOADS,
                                dir_default_download);
  }
  if (GetDefaultDownloadSafeDirectory(&dir_default_download_safe)) {
    base::PathService::Override(chrome::DIR_DEFAULT_DOWNLOADS_SAFE,
                                dir_default_download_safe);
  }
}

void OverrideUserDataDir(CefSettings* settings,
                         const base::CommandLine* command_line) {
  const base::FilePath& user_data_path =
      GetUserDataPath(settings, command_line);
  base::PathService::Override(chrome::DIR_USER_DATA, user_data_path);

  // Path used for crash dumps.
  base::PathService::Override(chrome::DIR_CRASH_DUMPS, user_data_path);

  // Path used for spell checking dictionary files.
  base::PathService::OverrideAndCreateIfNeeded(
      chrome::DIR_APP_DICTIONARIES, user_data_path.AppendASCII("Dictionaries"),
      false,  // May not be an absolute path.
      true);  // Create if necessary.
}

// Same as ui::ResourceBundle::IsScaleFactorSupported.
bool IsScaleFactorSupported(ui::ScaleFactor scale_factor) {
  const std::vector<ui::ScaleFactor>& supported_scale_factors =
      ui::GetSupportedScaleFactors();
  return std::find(supported_scale_factors.begin(),
                   supported_scale_factors.end(),
                   scale_factor) != supported_scale_factors.end();
}

#if defined(OS_LINUX)
void OverrideAssetPath() {
  Dl_info dl_info;
  if (dladdr(reinterpret_cast<const void*>(&OverrideAssetPath), &dl_info)) {
    base::FilePath path = base::FilePath(dl_info.dli_fname).DirName();
    base::PathService::Override(base::DIR_ASSETS, path);
  }
}
#endif

}  // namespace resource_util
