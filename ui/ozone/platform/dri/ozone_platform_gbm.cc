// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/ozone_platform_gbm.h"

#include <stdlib.h>
#include <gbm.h>

#include "ui/base/cursor/ozone/cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/gbm_surface_factory.h"
#include "ui/ozone/platform/dri/scanout_surface.h"
#include "ui/ozone/platform/dri/screen_manager.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#endif

namespace ui {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";

class GbmSurfaceGenerator : public ScanoutSurfaceGenerator {
 public:
  GbmSurfaceGenerator(DriWrapper* dri)
      : dri_(dri),
        device_(gbm_create_device(dri_->get_fd())) {}
  virtual ~GbmSurfaceGenerator() {
    gbm_device_destroy(device_);
  }

  gbm_device* device() const { return device_; }

  virtual ScanoutSurface* Create(const gfx::Size& size) OVERRIDE {
    return new GbmSurface(device_, dri_, size);
  }

 private:
  DriWrapper* dri_;  // Not owned.

  gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceGenerator);
};

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm() {}
  virtual ~OzonePlatformGbm() {}

  // OzonePlatform:
  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return surface_factory_ozone_.get();
  }
  virtual EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return event_factory_ozone_.get();
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return cursor_factory_ozone_.get();
  }
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }
#endif
  virtual void InitializeUI() OVERRIDE {
    // Needed since the browser process creates the accelerated widgets and that
    // happens through SFO.
    surface_factory_ozone_.reset(new GbmSurfaceFactory(NULL, NULL, NULL));

    device_manager_ = CreateDeviceManager();
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
    event_factory_ozone_.reset(new EventFactoryEvdev(
        NULL, device_manager_.get()));
  }

  virtual void InitializeGPU() OVERRIDE {
    dri_.reset(new DriWrapper(kDefaultGraphicsCardPath));
    surface_generator_.reset(new GbmSurfaceGenerator(dri_.get()));
    screen_manager_.reset(new ScreenManager(dri_.get(),
                                            surface_generator_.get()));
    surface_factory_ozone_.reset(
        new GbmSurfaceFactory(dri_.get(),
                              surface_generator_->device(),
                              screen_manager_.get()));
  }

 private:
  scoped_ptr<DriWrapper> dri_;
  scoped_ptr<GbmSurfaceGenerator> surface_generator_;
  // TODO(dnicoara) Move ownership of |screen_manager_| to NDD.
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<GbmSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() { return new OzonePlatformGbm; }

}  // namespace ui
