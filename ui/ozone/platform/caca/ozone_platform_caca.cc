// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/ozone_platform_caca.h"

#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/platform/caca/caca_event_source.h"
#include "ui/ozone/platform/caca/caca_window.h"
#include "ui/ozone/platform/caca/caca_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace {

class OzonePlatformCaca : public OzonePlatform {
 public:
  OzonePlatformCaca() {}
  ~OzonePlatformCaca() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return window_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return NULL;  // no GPU support
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return NULL;  // no GPU support
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    scoped_ptr<CacaWindow> caca_window(new CacaWindow(
        delegate, window_manager_.get(), event_source_.get(), bounds));
    if (!caca_window->Initialize())
      return nullptr;
    return caca_window.Pass();
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }

  void InitializeUI() override {
    window_manager_.reset(new CacaWindowManager);
    event_source_.reset(new CacaEventSource());
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
  }

  void InitializeGPU() override {}

 private:
  scoped_ptr<CacaWindowManager> window_manager_;
  scoped_ptr<CacaEventSource> event_source_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCaca() { return new OzonePlatformCaca; }

}  // namespace ui
