// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/ozone_platform_x11.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/x11/x11_surface_factory.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/x11/x11_window_ozone.h"

namespace ui {

namespace {

// Singleton OzonePlatform implementation for Linux X11 platform.
class OzonePlatformX11 : public OzonePlatform {
 public:
  OzonePlatformX11() {}
  ~OzonePlatformX11() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }

  ui::OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }

  scoped_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
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

  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    scoped_ptr<X11WindowOzone> window =
        make_scoped_ptr(new X11WindowOzone(event_source_.get(), delegate));
    window->SetBounds(bounds);
    window->Create();
    return std::move(window);
  }

  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(new NativeDisplayDelegateOzone());
  }

  base::ScopedFD OpenClientNativePixmapDevice() const override {
    return base::ScopedFD();
  }

  void InitializeUI() override {
    event_source_.reset(new X11EventSourceLibevent(gfx::GetXDisplay()));
    surface_factory_ozone_.reset(new X11SurfaceFactory());
    overlay_manager_.reset(new StubOverlayManager());
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU() override {
    surface_factory_ozone_.reset(new X11SurfaceFactory());
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  // Objects in the Browser process.
  scoped_ptr<X11EventSourceLibevent> event_source_;
  scoped_ptr<OverlayManagerOzone> overlay_manager_;
  scoped_ptr<InputController> input_controller_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  // Objects in the GPU process.
  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;

  // Objects in both Browser and GPU process.
  scoped_ptr<X11SurfaceFactory> surface_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformX11);
};

}  // namespace

OzonePlatform* CreateOzonePlatformX11() {
  return new OzonePlatformX11;
}

}  // namespace ui
