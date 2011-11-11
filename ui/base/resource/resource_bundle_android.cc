// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringprintf.h"

// We use a trick where we bundle the resource files in the apk
// as fake shared libraries. We should stop doing this as soon as either the
// resource files come pre-installed on the platform or there is a supported
// way to include additional files in the APK that get unpacked at install
// time.

namespace ui {

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  FilePath data_path;
  PathService::Get(base::DIR_MODULE, &data_path);
  DCHECK(!data_path.empty());
  return data_path.Append("lib_chrome.pak.so");
}

// static
FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale) {
  FilePath locale_path;
  PathService::Get(base::DIR_MODULE, &locale_path);
  DCHECK(!locale_path.empty());
  const std::string locale_name =
      StringPrintf("lib_%s.pak.so", app_locale.c_str());
  locale_path = locale_path.Append(locale_name);
  if (!file_util::PathExists(locale_path))
    return FilePath();
  return locale_path;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}

// static
FilePath ResourceBundle::GetLargeIconResourcesFilePath() {
  // Not supported.
  return FilePath();
}

}
