// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/ozone_platform_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "ui/base/cursor/ozone/cursor_factory_ozone.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/gfx/ozone/impl/file_surface_factory.h"
#include "ui/ozone/ime/input_method_context_factory_ozone.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/ozone_switches.h"

#if defined(OS_CHROMEOS)
#include "ui/ozone/common/chromeos/native_display_delegate_ozone.h"
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
  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() OVERRIDE {
    return surface_factory_ozone_.get();
  }
  virtual EventFactoryOzone* GetEventFactoryOzone() OVERRIDE {
    return event_factory_ozone_.get();
  }
  virtual InputMethodContextFactoryOzone* GetInputMethodContextFactoryOzone()
      OVERRIDE {
    return input_method_context_factory_ozone_.get();
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
    device_manager_ = CreateDeviceManager();
    surface_factory_ozone_.reset(new gfx::FileSurfaceFactory(file_path_));
    event_factory_ozone_.reset(
        new EventFactoryEvdev(NULL, device_manager_.get()));
    input_method_context_factory_ozone_.reset(
        new InputMethodContextFactoryOzone());
    cursor_factory_ozone_.reset(new CursorFactoryOzone());
  }

  virtual void InitializeGPU() OVERRIDE {}

 private:
  scoped_ptr<DeviceManager> device_manager_;
  scoped_ptr<gfx::FileSurfaceFactory> surface_factory_ozone_;
  scoped_ptr<EventFactoryEvdev> event_factory_ozone_;
  scoped_ptr<InputMethodContextFactoryOzone>
      input_method_context_factory_ozone_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformTest);
};

}  // namespace

OzonePlatform* CreateOzonePlatformTest() {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  base::FilePath location = base::FilePath("/dev/null");
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformTest(location);
}

}  // namespace ui
