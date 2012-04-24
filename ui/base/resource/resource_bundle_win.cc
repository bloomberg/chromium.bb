// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_win.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_data_dll_win.h"
#include "ui/base/win/dpi.h"

namespace ui {

namespace {

HINSTANCE resources_data_dll;

HINSTANCE GetCurrentResourceDLL() {
  if (resources_data_dll)
    return resources_data_dll;
  return GetModuleHandle(NULL);
}

FilePath GetResourcesPakFilePath(const std::string& pak_name) {
  FilePath path;
  if (PathService::Get(base::DIR_MODULE, &path))
    return path.AppendASCII(pak_name.c_str());
  return FilePath();
}

}  // end anonymous namespace

void ResourceBundle::LoadCommonResources() {
  // As a convenience, add the current resource module as a data packs.
  data_packs_.push_back(new ResourceDataDLL(GetCurrentResourceDLL()));

  bool use_hidpi_pak = false;
#if defined(ENABLE_HIDPI)
  // If we're running in HiDPI mode then use the 2x resource for DPI greater
  // than 1.5. Otherwise use the 1x resource.
  use_hidpi_pak = ui::GetDPIScale() > 1.5;
#endif

  if (!use_hidpi_pak) {
    AddDataPack(GetResourcesPakFilePath("theme_resources_standard.pak"));
    AddDataPack(GetResourcesPakFilePath("ui_resources_standard.pak"));
  } else {
    AddDataPack(GetResourcesPakFilePath("theme_resources_2x.pak"));
    AddDataPack(GetResourcesPakFilePath("ui_resources_2x.pak"));
  }
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id, ImageRTL rtl) {
  // Flipped image is not used on Windows.
  DCHECK_EQ(rtl, RTL_DISABLED);

  // Windows only uses SkBitmap for gfx::Image, so this is the same as
  // GetImageNamed.
  return GetImageNamed(resource_id);
}

void SetResourcesDataDLL(HINSTANCE handle) {
  resources_data_dll = handle;
}

HICON LoadThemeIconFromResourcesDataDLL(int icon_id) {
  return ::LoadIcon(GetCurrentResourceDLL(), MAKEINTRESOURCE(icon_id));
}

}  // namespace ui;
