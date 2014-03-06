// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_dri.h"

#include "ui/ozone/ozone_platform.h"

namespace ui {

OzonePlatformDri::OzonePlatformDri() {}

OzonePlatformDri::~OzonePlatformDri() {}

gfx::SurfaceFactoryOzone* OzonePlatformDri::GetSurfaceFactoryOzone() {
  return &surface_factory_ozone_;
}

ui::EventFactoryOzone* OzonePlatformDri::GetEventFactoryOzone() {
  return &event_factory_ozone_;
}

ui::InputMethodContextFactoryOzone*
OzonePlatformDri::GetInputMethodContextFactoryOzone() {
  return &input_method_context_factory_ozone_;
}

ui::CursorFactoryOzone* OzonePlatformDri::GetCursorFactoryOzone() {
  return &cursor_factory_ozone_;
}

OzonePlatform* CreateOzonePlatformDri() { return new OzonePlatformDri; }

}  // namespace ui
