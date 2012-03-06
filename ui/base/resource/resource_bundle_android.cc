// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringprintf.h"

namespace ui {

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  FilePath data_path;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &data_path);
  DCHECK(!data_path.empty());
  return data_path.Append(FILE_PATH_LITERAL("paks/chrome.pak"));
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
