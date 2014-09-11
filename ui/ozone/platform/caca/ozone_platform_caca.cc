// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/ozone_platform_caca.h"

#include "ui/ozone/platform/caca/caca_event_source.h"
#include "ui/ozone/platform/caca/caca_window.h"
#include "ui/ozone/platform/caca/caca_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#endif

namespace ui {

namespace {

class OzonePlatformCaca : public OzonePlatform {
 public:
  OzonePlatformCaca() {}
  virtual ~OzonePlatformCaca() {}

  // OzonePlatform:
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return window_manager_.get();
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return cursor_factory_ozone_.get();
  }
  virtual GpuPlatformSupport* GetGpuPlatformSupport() OVERRIDE {
    return NULL;  // no GPU support
  }
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() OVERRIDE {
    return NULL;  // no GPU support
  }
  virtual scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) OVERRIDE {
    scoped_ptr<CacaWindow> caca_window(new CacaWindow(
        delegate, window_manager_.get(), event_source_.get(), bounds));
    if (!caca_window->Initialize())
      return scoped_ptr<PlatformWindow>();
    return caca_window.PassAs<PlatformWindow>();
  }

#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }
#endif

  virtual void InitializeUI() OVERRIDE {
    window_manager_.reset(new CacaWindowManager);
    event_source_.reset(new CacaEventSource());
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
  }

  virtual void InitializeGPU() OVERRIDE {}

 private:
  scoped_ptr<CacaWindowManager> window_manager_;
  scoped_ptr<CacaEventSource> event_source_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCaca() { return new OzonePlatformCaca; }

}  // namespace ui
