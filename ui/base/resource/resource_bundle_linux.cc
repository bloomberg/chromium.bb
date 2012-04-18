// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/path_service.h"
#include "ui/base/ui_base_paths.h"

namespace {

FilePath GetResourcesPakFilePath(const std::string& pak_name) {
  FilePath path;
  if (PathService::Get(base::DIR_MODULE, &path))
    return path.AppendASCII(pak_name.c_str());
  return FilePath();
}

}  // namespace

namespace ui {

void ResourceBundle::LoadCommonResources() {
  AddDataPack(GetResourcesPakFilePath("chrome.pak"));
  AddDataPack(GetResourcesPakFilePath("theme_resources_standard.pak"));
  AddDataPack(GetResourcesPakFilePath("ui_resources_standard.pak"));
}

}  // namespace ui
