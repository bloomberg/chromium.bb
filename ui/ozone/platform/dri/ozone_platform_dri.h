// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_OZONE_PLATFORM_DRI_H_
#define UI_OZONE_PLATFORM_DRI_OZONE_PLATFORM_DRI_H_

#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/gfx/ozone/dri/dri_surface_factory.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/dri/cursor_factory_evdev_dri.h"

namespace ui {

// OzonePlatform for Linux DRI (Direct Rendering Infrastructure)
//
// This platform is Linux without any display server (no X, wayland, or
// anything). This means chrome alone owns the display and input devices.
class OzonePlatformDri : public OzonePlatform {
 public:
  OzonePlatformDri();
  virtual ~OzonePlatformDri();

  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE;
  virtual ui::EventFactoryOzone* GetEventFactoryOzone() OVERRIDE;
  virtual ui::InputMethodContextFactoryOzone*
      GetInputMethodContextFactoryOzone() OVERRIDE;
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE;
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<ui::NativeDisplayDelegate>
      CreateNativeDisplayDelegate() OVERRIDE;
#endif

 private:
  gfx::DriSurfaceFactory surface_factory_ozone_;
  ui::CursorFactoryEvdevDri cursor_factory_ozone_;
  ui::EventFactoryEvdev event_factory_ozone_;
  // This creates a minimal input context.
  ui::InputMethodContextFactoryOzone input_method_context_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDri);
};

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformDri();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_OZONE_PLATFORM_DRI_H_
