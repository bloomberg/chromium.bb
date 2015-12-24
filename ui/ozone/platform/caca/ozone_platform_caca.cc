// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/ozone_platform_caca.h"

#include "base/macros.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/no/no_keyboard_layout_engine.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/caca/caca_event_source.h"
#include "ui/ozone/platform/caca/caca_window.h"
#include "ui/ozone/platform/caca/caca_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#include "ui/ozone/public/system_input_injector.h"

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
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  InputController* GetInputController() override {
    return input_controller_.get();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  scoped_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;  // no input injection support.
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    scoped_ptr<CacaWindow> caca_window(new CacaWindow(
        delegate, window_manager_.get(), event_source_.get(), bounds));
    if (!caca_window->Initialize())
      return nullptr;
    return caca_window;
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(new NativeDisplayDelegateOzone());
  }
  base::ScopedFD OpenClientNativePixmapDevice() const override {
    return base::ScopedFD();
  }

  void InitializeUI() override {
    window_manager_.reset(new CacaWindowManager);
    overlay_manager_.reset(new StubOverlayManager());
    event_source_.reset(new CacaEventSource());
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
    input_controller_ = CreateStubInputController();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        make_scoped_ptr(new NoKeyboardLayoutEngine()));
  }

  void InitializeGPU() override {
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  scoped_ptr<CacaWindowManager> window_manager_;
  scoped_ptr<CacaEventSource> event_source_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  scoped_ptr<InputController> input_controller_;
  scoped_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCaca);
};

}  // namespace

OzonePlatform* CreateOzonePlatformCaca() {
  return new OzonePlatformCaca;
}

}  // namespace ui
