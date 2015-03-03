// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_dri.h"

#include "base/at_exit.h"
#include "base/thread_task_runner_handle.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/dri/display_manager.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_cursor.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_window.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/ozone/platform/dri/drm_device.h"
#include "ui/ozone/platform/dri/drm_device_generator.h"
#include "ui/ozone/platform/dri/drm_device_manager.h"
#include "ui/ozone/platform/dri/native_display_delegate_dri.h"
#include "ui/ozone/platform/dri/native_display_delegate_proxy.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
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
      : drm_(new DrmDevice(GetPrimaryDisplayCardPath())),
        buffer_generator_(new DriBufferGenerator()),
        screen_manager_(new ScreenManager(buffer_generator_.get())),
        device_manager_(CreateDeviceManager()),
        window_delegate_manager_() {}
  ~OzonePlatformDri() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  InputController* GetInputController() override {
    return event_factory_ozone_->input_controller();
  }
  GpuPlatformSupport* GetGpuPlatformSupport() override {
    return gpu_platform_support_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  scoped_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    scoped_ptr<DriWindow> platform_window(
        new DriWindow(delegate, bounds, gpu_platform_support_host_.get(),
                      event_factory_ozone_.get(), cursor_.get(),
                      window_manager_.get(), display_manager_.get()));
    platform_window->Initialize();
    return platform_window.Pass();
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(new NativeDisplayDelegateProxy(
        gpu_platform_support_host_.get(), device_manager_.get(),
        display_manager_.get(), drm_->device_path()));
  }
  void InitializeUI() override {
    if (!drm_->Initialize())
      LOG(FATAL) << "Failed to initialize primary DRM device";

    // This makes sure that simple targets that do not handle display
    // configuration can still use the primary display.
    ForceInitializationOfPrimaryDisplay(drm_, screen_manager_.get());
    drm_device_manager_.reset(new DrmDeviceManager(drm_));
    display_manager_.reset(new DisplayManager());
    surface_factory_ozone_.reset(
        new DriSurfaceFactory(&window_delegate_manager_));
    scoped_ptr<NativeDisplayDelegateDri> ndd(new NativeDisplayDelegateDri(
        screen_manager_.get(), drm_,
        scoped_ptr<DrmDeviceGenerator>(new DrmDeviceGenerator())));
    gpu_platform_support_.reset(new DriGpuPlatformSupport(
        drm_device_manager_.get(), &window_delegate_manager_,
        screen_manager_.get(), ndd.Pass()));
    gpu_platform_support_host_.reset(new DriGpuPlatformSupportHost());
    window_manager_.reset(new DriWindowManager());
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    cursor_.reset(
        new DriCursor(window_manager_.get(), gpu_platform_support_host_.get()));
    cursor_->Init();
#if defined(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(make_scoped_ptr(
        new XkbKeyboardLayoutEngine(xkb_evdev_code_converter_)));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        make_scoped_ptr(new StubKeyboardLayoutEngine()));
#endif
    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_.get(), device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()));

    if (!gpu_helper_.Initialize(base::ThreadTaskRunnerHandle::Get(),
                                base::ThreadTaskRunnerHandle::Get()))
      LOG(FATAL) << "Failed to initialize dummy channel.";
  }

  void InitializeGPU() override {}

 private:
  scoped_refptr<DrmDevice> drm_;
  scoped_ptr<DrmDeviceManager> drm_device_manager_;
  scoped_ptr<DriBufferGenerator> buffer_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<DriSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  scoped_ptr<DriCursor> cursor_;
  scoped_ptr<DriWindowManager> window_manager_;
  scoped_ptr<DisplayManager> display_manager_;

  scoped_ptr<DriGpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<DriGpuPlatformSupportHost> gpu_platform_support_host_;

  DriWindowDelegateManager window_delegate_manager_;

  OzoneGpuTestHelper gpu_helper_;

#if defined(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDri);
};

}  // namespace

OzonePlatform* CreateOzonePlatformDri() {
  return new OzonePlatformDri;
}

}  // namespace ui
