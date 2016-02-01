// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/wayland/wayland_display.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

namespace {

class OzonePlatformWayland : public OzonePlatform {
 public:
  OzonePlatformWayland() {}
  ~OzonePlatformWayland() override {}

  // OzonePlatform
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_.get();
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
    return nullptr;
  }

  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    auto window =
        make_scoped_ptr(new WaylandWindow(delegate, display_.get(), bounds));
    if (!window->Initialize())
      return nullptr;
    return std::move(window);
  }

  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(new NativeDisplayDelegateOzone);
  }

  base::ScopedFD OpenClientNativePixmapDevice() const override {
    NOTIMPLEMENTED();
    return base::ScopedFD();
  }

  void InitializeUI() override {
    display_.reset(new WaylandDisplay);
    if (!display_->Initialize())
      LOG(FATAL) << "Failed to initialize Wayland platform";

    cursor_factory_.reset(new CursorFactoryOzone);
    overlay_manager_.reset(new StubOverlayManager);
    input_controller_ = CreateStubInputController();
    surface_factory_.reset(new WaylandSurfaceFactory(display_.get()));
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU() override {
    // Don't reinitialize the surface factory in case InitializeUI was called
    // previously in the same process.
    if (!surface_factory_)
      surface_factory_.reset(new WaylandSurfaceFactory(nullptr));
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  scoped_ptr<WaylandDisplay> display_;
  scoped_ptr<WaylandSurfaceFactory> surface_factory_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_;
  scoped_ptr<StubOverlayManager> overlay_manager_;
  scoped_ptr<InputController> input_controller_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformWayland);
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
