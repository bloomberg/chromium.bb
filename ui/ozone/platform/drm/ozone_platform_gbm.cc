// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/ozone_platform_gbm.h"

#include <dlfcn.h>
#include <gbm.h>
#include <stdlib.h>

#include "base/bind.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_platform_support.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

#if defined(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

#if defined(USE_VGEM_MAP)
#include <fcntl.h>
#endif

namespace ui {

namespace {

class GlApiLoader {
 public:
  GlApiLoader()
      : glapi_lib_(dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL)) {}

  ~GlApiLoader() {
    if (glapi_lib_)
      dlclose(glapi_lib_);
  }

 private:
  // HACK: gbm drivers have broken linkage. The Mesa DRI driver references
  // symbols in the libglapi library however it does not explicitly link against
  // it. That caused linkage errors when running an application that does not
  // explicitly link against libglapi.
  void* glapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(GlApiLoader);
};

class GbmBufferGenerator : public ScanoutBufferGenerator {
 public:
  GbmBufferGenerator() {}
  ~GbmBufferGenerator() override {}

  // ScanoutBufferGenerator:
  scoped_refptr<ScanoutBuffer> Create(const scoped_refptr<DrmDevice>& drm,
                                      gfx::BufferFormat format,
                                      const gfx::Size& size) override {
    scoped_refptr<GbmDevice> gbm(static_cast<GbmDevice*>(drm.get()));
    return GbmBuffer::CreateBuffer(gbm, format, size,
                                   gfx::BufferUsage::SCANOUT);
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(GbmBufferGenerator);
};

class GbmDeviceGenerator : public DrmDeviceGenerator {
 public:
  GbmDeviceGenerator(bool use_atomic) : use_atomic_(use_atomic) {}
  ~GbmDeviceGenerator() override {}

  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::File file) override {
    scoped_refptr<DrmDevice> drm = new GbmDevice(path, file.Pass());
    if (drm->Initialize(use_atomic_))
      return drm;

    return nullptr;
  }

 private:
  bool use_atomic_;

  DISALLOW_COPY_AND_ASSIGN(GbmDeviceGenerator);
};

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm() {}
  ~OzonePlatformGbm() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
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
    scoped_ptr<DrmWindowHost> platform_window(
        new DrmWindowHost(delegate, bounds, gpu_platform_support_host_.get(),
                          event_factory_ozone_.get(), cursor_.get(),
                          window_manager_.get(), display_manager_.get()));
    platform_window->Initialize();
    return platform_window.Pass();
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(
        new DrmNativeDisplayDelegate(display_manager_.get()));
  }
  base::ScopedFD OpenClientNativePixmapDevice() const override {
#if defined(USE_VGEM_MAP)
    static const char kVgemPath[] = "/dev/dri/renderD129";
    base::ScopedFD vgem_fd(open(kVgemPath, O_RDWR | O_CLOEXEC));
    if (!vgem_fd.is_valid())
      PLOG(ERROR) << "Failed to open: " << kVgemPath;
    return vgem_fd;
#endif
    return base::ScopedFD();
  }
  void InitializeUI() override {
    device_manager_ = CreateDeviceManager();
    window_manager_.reset(new DrmWindowHostManager());
    cursor_.reset(new DrmCursor(window_manager_.get()));
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
    gpu_platform_support_host_.reset(
        new DrmGpuPlatformSupportHost(cursor_.get()));
    display_manager_.reset(new DrmDisplayHostManager(
        gpu_platform_support_host_.get(), device_manager_.get(),
        event_factory_ozone_->input_controller()));
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    overlay_manager_.reset(
        new DrmOverlayManager(gpu_platform_support_host_.get()));
  }

  void InitializeGPU() override {
    bool use_atomic = false;
#if defined(USE_DRM_ATOMIC)
    use_atomic = true;
#endif
    gl_api_loader_.reset(new GlApiLoader());
    drm_device_manager_.reset(new DrmDeviceManager(
        scoped_ptr<DrmDeviceGenerator>(new GbmDeviceGenerator(use_atomic))));
    buffer_generator_.reset(new GbmBufferGenerator());
    screen_manager_.reset(new ScreenManager(buffer_generator_.get()));
    surface_factory_ozone_.reset(new GbmSurfaceFactory());
    surface_factory_ozone_->InitializeGpu(drm_device_manager_.get(),
                                          screen_manager_.get());
    scoped_ptr<DrmGpuDisplayManager> display_manager(new DrmGpuDisplayManager(
        screen_manager_.get(), drm_device_manager_.get()));
    gpu_platform_support_.reset(new DrmGpuPlatformSupport(
        drm_device_manager_.get(), screen_manager_.get(),
        buffer_generator_.get(), display_manager.Pass()));
  }

 private:
  // Objects in the GPU process.
  scoped_ptr<GbmSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<GlApiLoader> gl_api_loader_;
  scoped_ptr<DrmDeviceManager> drm_device_manager_;
  scoped_ptr<GbmBufferGenerator> buffer_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DrmGpuPlatformSupport> gpu_platform_support_;

  // Objects in the Browser process.
  scoped_ptr<DeviceManager> device_manager_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<DrmWindowHostManager> window_manager_;
  scoped_ptr<DrmCursor> cursor_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;
  scoped_ptr<DrmGpuPlatformSupportHost> gpu_platform_support_host_;
  scoped_ptr<DrmDisplayHostManager> display_manager_;
  scoped_ptr<DrmOverlayManager> overlay_manager_;

#if defined(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() {
  return new OzonePlatformGbm;
}

}  // namespace ui
