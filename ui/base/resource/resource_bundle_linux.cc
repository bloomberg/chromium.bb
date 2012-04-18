// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/path_service.h"
#include "ui/base/ui_base_paths.h"

namespace ui {

void ResourceBundle::LoadCommonResources() {
  FilePath path;
  PathService::Get(ui::FILE_RESOURCES_PAK, &path);
  AddDataPack(path);
}

}  // namespace ui
