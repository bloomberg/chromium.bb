// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/ozone_platform_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/platform/test/test_window.h"
#include "ui/ozone/platform/test/test_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"

namespace ui {

namespace {

// OzonePlatform for testing
//
// This platform dumps images to a file for testing purposes.
class OzonePlatformTest : public OzonePlatform {
 public:
  OzonePlatformTest(const base::FilePath& dump_file) : file_path_(dump_file) {}
  ~OzonePlatformTest() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return window_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
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
    return nullptr;  // no input injection support.
  }
  scoped_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    return make_scoped_ptr<PlatformWindow>(
        new TestWindow(delegate, window_manager_.get(), bounds));
  }
  scoped_ptr<NativeDisplayDelegate> CreateNativeDisplayDelegate() override {
    return make_scoped_ptr(new NativeDisplayDelegateOzone());
  }

  void InitializeUI() override {
    window_manager_.reset(new TestWindowManager(file_path_));
    window_manager_->Initialize();
    // This unbreaks tests that create their own.
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = PlatformEventSource::CreateDefault();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        make_scoped_ptr(new StubKeyboardLayoutEngine()));

    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU() override {
    gpu_platform_support_.reset(CreateStubGpuPlatformSupport());
  }

 private:
  scoped_ptr<TestWindowManager> window_manager_;
  scoped_ptr<PlatformEventSource> platform_event_source_;
  scoped_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<InputController> input_controller_;
  scoped_ptr<GpuPlatformSupport> gpu_platform_support_;
  scoped_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformTest);
};

}  // namespace

OzonePlatform* CreateOzonePlatformTest() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::FilePath location;
  if (cmd->HasSwitch(switches::kOzoneDumpFile))
    location = cmd->GetSwitchValuePath(switches::kOzoneDumpFile);
  return new OzonePlatformTest(location);
}

}  // namespace ui
