// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image.h"

namespace ui {

void ResourceBundle::LoadCommonResources() {
  LoadChromeResources();
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id, ImageRTL rtl) {
  // Flipped image is not used on ChromeOS.
  DCHECK_EQ(rtl, RTL_DISABLED);
  return GetImageNamed(resource_id);
}

}  // namespace ui
