// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/ozone_platform_x11.h"

#include <X11/Xlib.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/fake_display_delegate.h"
#include "ui/events/platform/x11/x11_event_source_libevent.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/x11/x11_cursor_factory_ozone.h"
#include "ui/ozone/platform/x11/x11_surface_factory.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/x11/x11_window_manager_ozone.h"
#include "ui/platform_window/x11/x11_window_ozone.h"

namespace ui {

namespace {

// Returns true if a flag is present that will cause Ozone UI and GPU to run in
// the same process.
// TODO(kylechar): Remove --mojo-platform-channel-handle when mus-ws process
// split happens.
bool HasSingleProcessFlag() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch("mojo-platform-channel-handle") ||
         command_line->HasSwitch("single-process");
}

// Singleton OzonePlatform implementation for X11 platform.
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

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    std::unique_ptr<X11WindowOzone> window = base::MakeUnique<X11WindowOzone>(
        window_manager_.get(), delegate, bounds);
    window->Create();
    window->SetTitle(base::ASCIIToUTF16("Ozone X11"));
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::MakeUnique<display::FakeDisplayDelegate>();
  }

  void InitializeUI(const InitParams& params) override {
    InitializeCommon(params);
    CreatePlatformEventSource();
    window_manager_ = base::MakeUnique<X11WindowManagerOzone>();
    overlay_manager_ = base::MakeUnique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_ = base::MakeUnique<X11CursorFactoryOzone>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU(const InitParams& params) override {
    InitializeCommon(params);

    // In single process mode either the UI thread will create an event source
    // or it's a test and an event source isn't desired.
    if (!params.single_process && !HasSingleProcessFlag())
      CreatePlatformEventSource();

    surface_factory_ozone_ = base::MakeUnique<X11SurfaceFactory>();
  }

  base::MessageLoop::Type GetMessageLoopTypeForGpu() override {
    // When Ozone X11 backend is running use an UI loop to grab Expose events.
    // See GLSurfaceGLX and https://crbug.com/326995.
    return base::MessageLoop::TYPE_UI;
  }

 private:
  // Performs initialization steps need by both UI and GPU.
  void InitializeCommon(const InitParams& params) {
    // TODO(kylechar): Add DCHECK we only enter InitializeCommon() twice for
    // single process mode.
    if (common_initialized_)
      return;

    // In single process mode XInitThreads() must be the first Xlib call.
    if (params.single_process || HasSingleProcessFlag())
      XInitThreads();

    ui::SetDefaultX11ErrorHandlers();

    common_initialized_ = true;
  }

  // Creates |event_source_| if it doesn't already exist.
  void CreatePlatformEventSource() {
    if (event_source_)
      return;

    XDisplay* display = gfx::GetXDisplay();
    event_source_ = base::MakeUnique<X11EventSourceLibevent>(display);
  }

  bool common_initialized_ = false;

  // Objects in the UI process.
  std::unique_ptr<X11WindowManagerOzone> window_manager_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<X11CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  // Objects in the GPU process.
  std::unique_ptr<X11SurfaceFactory> surface_factory_ozone_;

  // Objects in both UI and GPU process.
  std::unique_ptr<X11EventSourceLibevent> event_source_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformX11);
};

}  // namespace

OzonePlatform* CreateOzonePlatformX11() {
  return new OzonePlatformX11;
}

}  // namespace ui
