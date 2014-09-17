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
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_window.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/platform/dri/virtual_terminal_manager.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"
#endif

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
      : vt_manager_(new VirtualTerminalManager()),
        dri_(new DriWrapper(kDefaultGraphicsCardPath)),
        buffer_generator_(new DriBufferGenerator(dri_.get())),
        screen_manager_(new ScreenManager(dri_.get(),
                                          buffer_generator_.get())),
        device_manager_(CreateDeviceManager()) {
    base::AtExitManager::RegisterTask(
        base::Bind(&base::DeletePointer<OzonePlatformDri>, this));
  }
  virtual ~OzonePlatformDri() {}

  // OzonePlatform:
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return surface_factory_ozone_.get();
  }
  virtual CursorFactoryOzone* GetCursorFactoryOzone() OVERRIDE {
    return cursor_factory_ozone_.get();
  }
  virtual GpuPlatformSupport* GetGpuPlatformSupport() OVERRIDE {
    return NULL;  // no GPU support
  }
  virtual GpuPlatformSupportHost* GetGpuPlatformSupportHost() OVERRIDE {
    return NULL;  // no GPU support
  }
  virtual scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) OVERRIDE {
    scoped_ptr<DriWindow> platform_window(new DriWindow(
        delegate,
        bounds,
        scoped_ptr<DriWindowDelegate>(new DriWindowDelegateImpl(
            window_manager_->NextAcceleratedWidget(), screen_manager_.get())),
        event_factory_ozone_.get(),
        &window_delegate_manager_,
        window_manager_.get()));
    platform_window->Initialize();
    return platform_window.PassAs<PlatformWindow>();
  }
#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateDri(
        dri_.get(), screen_manager_.get(), device_manager_.get()));
  }
#endif
  virtual void InitializeUI() OVERRIDE {
    dri_->Initialize();
    surface_factory_ozone_.reset(new DriSurfaceFactory(
        dri_.get(), screen_manager_.get(), &window_delegate_manager_));
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    window_manager_.reset(new DriWindowManager(surface_factory_ozone_.get()));
    event_factory_ozone_.reset(new EventFactoryEvdev(window_manager_->cursor(),
                                                     device_manager_.get()));
    if (surface_factory_ozone_->InitializeHardware() !=
        DriSurfaceFactory::INITIALIZED)
      LOG(FATAL) << "failed to initialize display hardware";

  }

  virtual void InitializeGPU() OVERRIDE {}

 private:
  scoped_ptr<VirtualTerminalManager> vt_manager_;
  scoped_ptr<DriWrapper> dri_;
  scoped_ptr<DriBufferGenerator> buffer_generator_;
  scoped_ptr<ScreenManager> screen_manager_;
  scoped_ptr<DeviceManager> device_manager_;

  scoped_ptr<DriSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;

  scoped_ptr<DriWindowManager> window_manager_;
  DriWindowDelegateManager window_delegate_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformDri);
};

}  // namespace

OzonePlatform* CreateOzonePlatformDri() { return new OzonePlatformDri; }

}  // namespace ui
