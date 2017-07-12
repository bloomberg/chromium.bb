// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/logging.h"
#include "base/macros.h"
#include "ui/gfx/image/image.h"

namespace ui {

// TODO(fuchsia): Implement ui::ResourceBundle.

void ResourceBundle::LoadCommonResources() {
  NOTIMPLEMENTED();
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  NOTIMPLEMENTED();
  CR_DEFINE_STATIC_LOCAL(gfx::Image, empty_image, ());
  return empty_image;
}

}  // namespace ui
