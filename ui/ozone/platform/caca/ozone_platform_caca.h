// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_OZONE_PLATFORM_CACA_H_
#define UI_OZONE_PLATFORM_CACA_OZONE_PLATFORM_CACA_H_

#include "ui/base/cursor/ozone/cursor_factory_ozone.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/caca/caca_connection.h"
#include "ui/ozone/platform/caca/caca_event_factory.h"
#include "ui/ozone/platform/caca/caca_surface_factory.h"

namespace ui {

class CacaConnection;

class OzonePlatformCaca : public OzonePlatform {
 public:
  OzonePlatformCaca();
  virtual ~OzonePlatformCaca();

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
  ui::CacaConnection connection_;
  ui::CacaSurfaceFactory surface_factory_ozone_;
  ui::CacaEventFactory event_factory_ozone_;
  // This creates a minimal input context.
  ui::InputMethodContextFactoryOzone input_method_context_factory_ozone_;
  ui::CursorFactoryOzone cursor_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

// Constructor hook for use in ozone_platform_list.cc
OZONE_EXPORT OzonePlatform* CreateOzonePlatformCaca();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_OZONE_PLATFORM_CACA_H_
