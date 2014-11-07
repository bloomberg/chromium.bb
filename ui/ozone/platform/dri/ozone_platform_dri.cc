// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_dri.h"

#include "base/at_exit.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_window.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/native_display_delegate_dri.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ui_thread_gpu.h"

namespace ui {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";

// OzonePlatform for Linux DRI (Direct Rendering Infrastructure)
//
// This platform is Linux without any display server (no X, wayland, or
// anything). This means chrome alone owns the display and input devices.
class OzonePlatformDri : public OzonePlatform {
 public:
  OzonePlatformDri()
      : dri_(new DriWrapper(kDefaultGraphicsCardPath)),
        buffer_generator_(new DriBufferGenerator(dri_.get())),
        screen_manager_(new ScreenManager(dri_.get(), buffer_generator_.get())),
        device_manager_(CreateDeviceManager()) {
    base::AtExitManager::RegisterTask(
        base::Bind(&base::DeletePointer<OzonePlatformDri>, this));
  }
  ~OzonePlatformDri() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
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
    scoped_ptr<DriWindow> platform_window(
        new DriWindow(delegate, bounds, gpu_platform_support_host_.get(),
                      event_factory_ozone_.get(), window_manager_.get()));
    platform_window->Initialize();
    return platform_window.Pass();
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateDri(
        dri_.get(), screen_manager_.get(), device_manager_.get()));
  }
  void InitializeUI() override {
    dri_->Initialize();
    surface_factory_ozone_.reset(new DriSurfaceFactory(
        dri_.get(), screen_manager_.get(), &window_delegate_manager_));
    gpu_platform_support_.reset(new DriGpuPlatformSupport(
        surface_factory_ozone_.get(), &window_delegate_manager_,
        screen_manager_.get(), scoped_ptr<NativeDisplayDelegateDri>()));
    gpu_platform_support_host_.reset(new DriGpuPlatformSupportHost());
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    window_manager_.reset(new DriWindowManager(surface_factory_ozone_.get()));
    event_factory_ozone_.reset(new EventFactoryEvdev(window_manager_->cursor(),
                                                     device_manager_.get()));
    if (surface_factory_ozone_->InitializeHardware() !=
        DriSurfaceFactory::INITIALIZED)
      LOG(FATAL) << "Failed to initialize display hardware.";

    if (!ui_thread_gpu_.Initialize())
      LOG(FATAL) << "Failed to initialize dummy channel.";
  }

  void InitializeGPU() override {}

 private:
  scoped_ptr<DriWrapper> dri_;
  scoped_ptr<DriBufferGenerator> buffer_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<DriSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  scoped_ptr<DriWindowManager> window_manager_;

  scoped_ptr<DriGpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<DriGpuPlatformSupportHost> gpu_platform_support_host_;

  DriWindowDelegateManager window_delegate_manager_;

  UiThreadGpu ui_thread_gpu_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDri);
};

}  // namespace

OzonePlatform* CreateOzonePlatformDri() { return new OzonePlatformDri; }

}  // namespace ui
