// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_dri.h"

#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/ime/input_method_context_factory_ozone.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/dri/cursor_factory_evdev_dri.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"
#endif

namespace ui {

namespace {

// OzonePlatform for Linux DRI (Direct Rendering Infrastructure)
//
// This platform is Linux without any display server (no X, wayland, or
// anything). This means chrome alone owns the display and input devices.
class OzonePlatformDri : public OzonePlatform {
 public:
  OzonePlatformDri()
      : cursor_factory_ozone_(&surface_factory_ozone_),
        event_factory_ozone_(&cursor_factory_ozone_) {}
  virtual ~OzonePlatformDri() {}

  // OzonePlatform:
  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return &surface_factory_ozone_;
  }
  virtual ui::EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return &event_factory_ozone_;
  }
  virtual ui::InputMethodContextFactoryOzone*
  GetInputMethodContextFactoryOzone() OVERRIDE {
    return &input_method_context_factory_ozone_;
  }
  virtual ui::CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return &cursor_factory_ozone_;
  }
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<ui::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<ui::NativeDisplayDelegate>(
        new NativeDisplayDelegateDri(&surface_factory_ozone_));
  }
#endif

 private:
  ui::DriSurfaceFactory surface_factory_ozone_;
  ui::CursorFactoryEvdevDri cursor_factory_ozone_;
  ui::EventFactoryEvdev event_factory_ozone_;
  // This creates a minimal input context.
  ui::InputMethodContextFactoryOzone input_method_context_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDri);
};

}  // namespace

OzonePlatform* CreateOzonePlatformDri() { return new OzonePlatformDri; }

}  // namespace ui
