// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_win.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "ui/base/layout.h"
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

  // Return just the name of the pack file.
  return FilePath(ASCIIToUTF16(pak_name));
}

}  // end anonymous namespace

void ResourceBundle::LoadCommonResources() {
  // As a convenience, add the current resource module as a data packs.
  data_packs_.push_back(new ResourceDataDLL(GetCurrentResourceDLL()));

  bool use_hidpi = false;
#if defined(ENABLE_HIDPI)
  // If we're running in HiDPI mode at a scale larger than 150%, we switch
  // to 2x resources for desktop layouts.
  use_hidpi = ui::GetDPIScale() > 1.5;
#endif

  switch (ui::GetDisplayLayout()) {
    case ui::LAYOUT_TOUCH:
      AddDataPack(GetResourcesPakFilePath("theme_resources_touch_1x.pak"),
                  SCALE_FACTOR_100P);
      AddDataPack(GetResourcesPakFilePath("ui_resources_standard.pak"),
                  SCALE_FACTOR_100P);
      break;
    default:
      if (use_hidpi) {
        AddDataPack(GetResourcesPakFilePath("theme_resources_2x.pak"),
                    SCALE_FACTOR_200P);
        AddDataPack(GetResourcesPakFilePath("ui_resources_2x.pak"),
                    SCALE_FACTOR_200P);
      } else {
        AddDataPack(GetResourcesPakFilePath("theme_resources_standard.pak"),
                    SCALE_FACTOR_100P);
        AddDataPack(GetResourcesPakFilePath("ui_resources_standard.pak"),
                    SCALE_FACTOR_100P);
      }
      break;
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
