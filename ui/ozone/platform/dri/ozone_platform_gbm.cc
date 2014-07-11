// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_gbm.h"

#include <dlfcn.h>
#include <gbm.h>
#include <stdlib.h>

#include "base/at_exit.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/platform/dri/cursor_factory_evdev_dri.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/gbm_surface_factory.h"
#include "ui/ozone/platform/dri/gpu_platform_support_gbm.h"
#include "ui/ozone/platform/dri/gpu_platform_support_host_gbm.h"
#include "ui/ozone/platform/dri/scanout_surface.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/virtual_terminal_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/touchscreen_device_manager_ozone.h"
#include "ui/ozone/platform/dri/chromeos/display_message_handler.h"
#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"
#include "ui/ozone/platform/dri/chromeos/native_display_delegate_proxy.h"
#endif

namespace ui {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";

class GbmSurfaceGenerator : public ScanoutSurfaceGenerator {
 public:
  GbmSurfaceGenerator(DriWrapper* dri)
      : dri_(dri),
        glapi_lib_(dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL)),
        device_(gbm_create_device(dri_->get_fd())) {}
  virtual ~GbmSurfaceGenerator() {
    gbm_device_destroy(device_);
    if (glapi_lib_)
      dlclose(glapi_lib_);
  }

  gbm_device* device() const { return device_; }

  virtual ScanoutSurface* Create(const gfx::Size& size) OVERRIDE {
    return new GbmSurface(device_, dri_, size);
  }

 protected:
  DriWrapper* dri_;  // Not owned.

  // HACK: gbm drivers have broken linkage
  void *glapi_lib_;

  gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceGenerator);
};

class GbmEglImageSurfaceGenerator : public GbmSurfaceGenerator {
 public:
  GbmEglImageSurfaceGenerator(DriWrapper* dri) : GbmSurfaceGenerator(dri) {}
  virtual ~GbmEglImageSurfaceGenerator() {}

  virtual ScanoutSurface* Create(const gfx::Size& size) OVERRIDE {
    scoped_ptr<GbmBuffer> buffer =
        scoped_ptr<GbmBuffer>(new GbmBuffer(device_, dri_, size));
    if (!buffer->InitializeBuffer(SurfaceFactoryOzone::RGBA_8888, true)) {
      return NULL;
    }
    return buffer.release();
  }
};

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm(bool use_surfaceless) : use_surfaceless_(use_surfaceless) {
     base::AtExitManager::RegisterTask(
        base::Bind(&base::DeletePointer<OzonePlatformGbm>, this));
  }
  virtual ~OzonePlatformGbm() {}

  // OzonePlatform:
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return surface_factory_ozone_.get();
  }
  virtual EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return event_factory_ozone_.get();
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return cursor_factory_ozone_.get();
  }
  virtual GpuPlatformSupport* GetGpuPlatformSupport() OVERRIDE {
    return gpu_platform_support_.get();
  }
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() OVERRIDE {
    return gpu_platform_support_host_.get();
  }
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateProxy(
        gpu_platform_support_host_.get(), device_manager_.get()));
  }
  virtual scoped_ptr<TouchscreenDeviceManager>
      CreateTouchscreenDeviceManager() OVERRIDE {
    return scoped_ptr<TouchscreenDeviceManager>(
        new TouchscreenDeviceManagerOzone());
  }
#endif
  virtual void InitializeUI() OVERRIDE {
    vt_manager_.reset(new VirtualTerminalManager());
    // Needed since the browser process creates the accelerated widgets and that
    // happens through SFO.
    surface_factory_ozone_.reset(new GbmSurfaceFactory(use_surfaceless_));

    device_manager_ = CreateDeviceManager();
    gpu_platform_support_host_.reset(new GpuPlatformSupportHostGbm());
    cursor_factory_ozone_.reset(
        new CursorFactoryEvdevDri(gpu_platform_support_host_.get()));
    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_factory_ozone_.get(), device_manager_.get()));
  }

  virtual void InitializeGPU() OVERRIDE {
    dri_.reset(new DriWrapper(kDefaultGraphicsCardPath));
    if (use_surfaceless_)
      surface_generator_.reset(new GbmEglImageSurfaceGenerator(dri_.get()));
    else
      surface_generator_.reset(new GbmSurfaceGenerator(dri_.get()));
    screen_manager_.reset(new ScreenManager(dri_.get(),
                                            surface_generator_.get()));
    if (!surface_factory_ozone_)
      surface_factory_ozone_.reset(new GbmSurfaceFactory(use_surfaceless_));

    surface_factory_ozone_->InitializeGpu(dri_.get(),
                                          surface_generator_->device(),
                                          screen_manager_.get());

    gpu_platform_support_.reset(
        new GpuPlatformSupportGbm(surface_factory_ozone_.get()));
#if defined(OS_CHROMEOS)
    gpu_platform_support_->AddHandler(scoped_ptr<GpuPlatformSupport>(
        new DisplayMessageHandler(
            scoped_ptr<NativeDisplayDelegateDri>(new NativeDisplayDelegateDri(
                dri_.get(),
                screen_manager_.get(),
                NULL)))));
#endif
  }

 private:
  bool use_surfaceless_;
  scoped_ptr<VirtualTerminalManager> vt_manager_;
  scoped_ptr<DriWrapper> dri_;
  scoped_ptr<GbmSurfaceGenerator> surface_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<GbmSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<CursorFactoryEvdevDri> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  scoped_ptr<GpuPlatformSupportGbm> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHostGbm> gpu_platform_support_host_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() {
  return new OzonePlatformGbm(false);
}
OzonePlatform* CreateOzonePlatformGbmEglImage() {
  return new OzonePlatformGbm(true);
}

}  // namespace ui
