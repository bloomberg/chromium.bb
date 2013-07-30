// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/resources.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace remoting {

namespace {
const char kLocaleResourcesDirName[] = "remoting_locales";
const char kCommonResourcesFileName[] = "chrome_remote_desktop.pak";
}  // namespace

// Loads chromoting resources.
bool LoadResources(const std::string& pref_locale) {
  base::FilePath path;
  if (!PathService::Get(base::DIR_MODULE, &path))
    return false;

  PathService::Override(ui::DIR_LOCALES,
                        path.AppendASCII(kLocaleResourcesDirName));
  ui::ResourceBundle::InitSharedInstanceLocaleOnly(pref_locale, NULL);

  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      path.AppendASCII(kCommonResourcesFileName), ui::SCALE_FACTOR_100P);

  return true;
}

}  // namespace remoting
