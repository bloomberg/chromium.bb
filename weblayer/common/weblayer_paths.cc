// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/common/weblayer_paths.h"

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
#endif

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#endif

namespace weblayer {

namespace {

void CreateDir(const base::FilePath& path) {
  if (!base::PathExists(path))
    base::CreateDirectory(path);
}

bool GetDefaultUserDataDirectory(base::FilePath* result) {
#if defined(OS_ANDROID)
  // No need to append "weblayer" here. It's done in java with
  // PathUtils.setPrivateDataDirectorySuffix.
  return base::PathService::Get(base::DIR_ANDROID_APP_DATA, result);
#elif defined(OS_WIN)
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  *result = result->AppendASCII("weblayer");
  return true;
#elif defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath config_dir(base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
  *result = config_dir.AppendASCII("weblayer");
  return true;
#else
  return false;
#endif
}

}  // namespace

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;

  switch (key) {
    case DIR_USER_DATA: {
      bool rv = GetDefaultUserDataDirectory(result);
      if (rv)
        CreateDir(*result);
      return rv;
    }
#if defined(OS_ANDROID)
    case weblayer::DIR_CRASH_DUMPS:
      if (!base::android::GetCacheDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Crashpad"));
      CreateDir(cur);
      *result = cur;
      return true;
#endif
    default:
      return false;
  }
}

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace weblayer
