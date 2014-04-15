// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_dri.h"

#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/ozone/ozone_platform.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#endif

namespace ui {

OzonePlatformDri::OzonePlatformDri()
    : cursor_factory_ozone_(&surface_factory_ozone_),
      event_factory_ozone_(&cursor_factory_ozone_) {}

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

#if defined(OS_CHROMEOS)
scoped_ptr<ui::NativeDisplayDelegate>
OzonePlatformDri::CreateNativeDisplayDelegate() {
  return scoped_ptr<ui::NativeDisplayDelegate>(
      new NativeDisplayDelegateOzone());
}
#endif

OzonePlatform* CreateOzonePlatformDri() { return new OzonePlatformDri; }

}  // namespace ui
