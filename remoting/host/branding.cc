// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/branding.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace {

// TODO(lambroslambrou): The default locations should depend on whether Chrome
// branding is enabled - this means also modifying the Python daemon script.
// The actual location of the files is ultimately determined by the service
// daemon and native messaging host - these defaults are only used in case the
// command-line switches are absent.
#if defined(OS_WIN)
#ifdef OFFICIAL_BUILD
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("Google\\Chrome Remote Desktop");
#else
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("Chromoting");
#endif
#elif defined(OS_MACOSX)
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("Chrome Remote Desktop");
#else
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL(".config/chrome-remote-desktop");
#endif

} // namespace

namespace remoting {

#if defined(OS_WIN)
const wchar_t kWindowsServiceName[] = L"chromoting";
#endif

base::FilePath GetConfigDir() {
  base::FilePath app_data_dir;

#if defined(OS_WIN)
  base::PathService::Get(base::DIR_COMMON_APP_DATA, &app_data_dir);
#elif defined(OS_MACOSX)
  base::PathService::Get(base::DIR_APP_DATA, &app_data_dir);
#else
  base::PathService::Get(base::DIR_HOME, &app_data_dir);
#endif

  return app_data_dir.Append(kConfigDir);
}

} // namespace remoting
