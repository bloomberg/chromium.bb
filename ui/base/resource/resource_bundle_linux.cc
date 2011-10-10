// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/path_service.h"
#include "ui/base/ui_base_paths.h"

namespace ui {

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  FilePath resources_file_path;
  PathService::Get(ui::FILE_RESOURCES_PAK, &resources_file_path);
  return resources_file_path;
}

// static
FilePath ResourceBundle::GetLargeIconResourcesFilePath() {
  // Not supported.
  return FilePath();
}

}  // namespace ui
