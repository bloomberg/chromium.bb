// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/platform_support.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "grit/webkit_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/support/test_webkit_platform_support.h"

namespace webkit_support {

// TODO(tkent): Implement some of the followings for platform-dependent tasks
// such as loading resource.

void BeforeInitialize(bool unit_test_mode) {
}

void AfterInitialize(bool unit_test_mode) {
  if (unit_test_mode)
    return;  // We don't have a resource pack when running the unit-tests.

  FilePath data_path;
  PathService::Get(base::DIR_EXE, &data_path);
  data_path = data_path.Append("DumpRenderTree.pak");
  ResourceBundle::InitSharedInstanceWithPakFile(data_path);
}

void BeforeShutdown() {
  ResourceBundle::CleanupSharedInstance();
}

void AfterShutdown() {
}

}  // namespace webkit_support

string16 TestWebKitPlatformSupport::GetLocalizedString(int message_id) {
  return ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

base::StringPiece TestWebKitPlatformSupport::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  FilePath resources_path;
  PathService::Get(base::DIR_EXE, &resources_path);
  resources_path = resources_path.Append("DumpRenderTree_resources");
  switch (resource_id) {
    case IDR_BROKENIMAGE: {
      CR_DEFINE_STATIC_LOCAL(std::string, broken_image_data, ());
      if (broken_image_data.empty()) {
        FilePath path = resources_path.Append("missingImage.gif");
        bool success = file_util::ReadFileToString(path, &broken_image_data);
        if (!success)
          LOG(FATAL) << "Failed reading: " << path.value();
      }
      return broken_image_data;
    }
    case IDR_TEXTAREA_RESIZER: {
      CR_DEFINE_STATIC_LOCAL(std::string, resize_corner_data, ());
      if (resize_corner_data.empty()) {
        FilePath path = resources_path.Append("textAreaResizeCorner.png");
        bool success = file_util::ReadFileToString(path, &resize_corner_data);
        if (!success)
          LOG(FATAL) << "Failed reading: " << path.value();
      }
      return resize_corner_data;
    }
  }

  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      resource_id, scale_factor);
}
