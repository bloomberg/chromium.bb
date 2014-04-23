// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/ozone_platform_caca.h"

#include "ui/base/cursor/ozone/cursor_factory_ozone.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/caca/caca_connection.h"
#include "ui/ozone/platform/caca/caca_event_factory.h"
#include "ui/ozone/platform/caca/caca_surface_factory.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#endif

namespace ui {

namespace {

class OzonePlatformCaca : public OzonePlatform {
 public:
  OzonePlatformCaca()
      : surface_factory_ozone_(&connection_),
        event_factory_ozone_(&connection_) {}
  virtual ~OzonePlatformCaca() {}

  // OzonePlatform:
  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return &surface_factory_ozone_;
  }
  virtual EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return &event_factory_ozone_;
  }
  virtual InputMethodContextFactoryOzone* GetInputMethodContextFactoryOzone()
      OVERRIDE {
    return &input_method_context_factory_ozone_;
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return &cursor_factory_ozone_;
  }

#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }
#endif

 private:
  CacaConnection connection_;
  CacaSurfaceFactory surface_factory_ozone_;
  CacaEventFactory event_factory_ozone_;
  // This creates a minimal input context.
  InputMethodContextFactoryOzone input_method_context_factory_ozone_;
  CursorFactoryOzone cursor_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCaca() { return new OzonePlatformCaca; }

}  // namespace ui
