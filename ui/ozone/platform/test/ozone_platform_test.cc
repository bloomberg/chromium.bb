// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/ozone_platform_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/test/test_cursor_factory.h"
#include "ui/ozone/platform/test/test_window.h"
#include "ui/ozone/platform/test/test_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
#include "ui/ozone/common/chromeos/touchscreen_device_manager_ozone.h"
#endif

namespace ui {

namespace {

// OzonePlatform for testing
//
// This platform dumps images to a file for testing purposes.
class OzonePlatformTest : public OzonePlatform {
 public:
  OzonePlatformTest(const base::FilePath& dump_file) : file_path_(dump_file) {}
  virtual ~OzonePlatformTest() {}

  // OzonePlatform:
  virtual ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return window_manager_.get();
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
  virtual scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) OVERRIDE {
    return make_scoped_ptr<PlatformWindow>(
        new TestWindow(delegate, window_manager_.get(), bounds));
  }

#if defined(OS_CHROMEOS)
  virtual scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate()
      OVERRIDE {
    return scoped_ptr<NativeDisplayDelegate>(new NativeDisplayDelegateOzone());
  }
  virtual scoped_ptr<TouchscreenDeviceManager>
      CreateTouchscreenDeviceManager() OVERRIDE {
    return scoped_ptr<TouchscreenDeviceManager>(
        new TouchscreenDeviceManagerOzone());
  }
#endif

  virtual void InitializeUI() OVERRIDE {
    window_manager_.reset(new TestWindowManager(file_path_));
    window_manager_->Initialize();
    // This unbreaks tests that create their own.
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = PlatformEventSource::CreateDefault();

    cursor_factory_ozone_.reset(new TestCursorFactory());
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  virtual void InitializeGPU() OVERRIDE {
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  scoped_ptr<TestWindowManager> window_manager_;
  scoped_ptr<PlatformEventSource> platform_event_source_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformTest);
};

}  // namespace

OzonePlatform* CreateOzonePlatformTest() {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  base::FilePath location;
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformTest(location);
}

}  // namespace ui
