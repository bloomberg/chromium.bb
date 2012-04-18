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

void ResourceBundle::LoadCommonResources() {
  FilePath data_path;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &data_path);
  DCHECK(!data_path.empty());
  data_path = data_path.AppendASCII("paks").AppendASCII("chrome.pak");
  AddDataPack(data_path);
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id, ImageRTL rtl) {
  // Flipped image is not used on Android.
  DCHECK_EQ(rtl, RTL_DISABLED);
  return GetImageNamed(resource_id);
}

}
