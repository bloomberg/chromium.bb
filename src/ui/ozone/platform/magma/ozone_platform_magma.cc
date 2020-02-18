// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/magma/ozone_platform_magma.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/fake/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/magma/magma_surface_factory.h"
#include "ui/ozone/platform/magma/magma_window.h"
#include "ui/ozone/platform/magma/magma_window_manager.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

// Not implemented for now.
class MagmaPlatformEventSource : public ui::PlatformEventSource {
 public:
  MagmaPlatformEventSource() = default;
  ~MagmaPlatformEventSource() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MagmaPlatformEventSource);
};

// OzonePlatform for Magma
class OzonePlatformMagma : public OzonePlatform {
 public:
  OzonePlatformMagma() {}

  ~OzonePlatformMagma() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }
  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }
  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }
  InputController* GetInputController() override {
    return input_controller_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;  // no input injection support.
  }
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    return std::make_unique<MagmaWindow>(delegate, window_manager_.get(),
                                         properties.bounds);
  }
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<display::FakeDisplayDelegate>();
  }
  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate) override {
    return std::make_unique<InputMethodMinimal>(delegate);
  }

  void InitializeUI(const InitParams& params) override {
    window_manager_ = std::make_unique<MagmaWindowManager>();
    surface_factory_ = std::make_unique<MagmaSurfaceFactory>();
    // This unbreaks tests that create their own.
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = std::make_unique<MagmaPlatformEventSource>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<StubKeyboardLayoutEngine>());

    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_ = std::make_unique<BitmapCursorFactoryOzone>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU(const InitParams& params) override {
    if (!surface_factory_)
      surface_factory_ = std::make_unique<MagmaSurfaceFactory>();
  }

 private:
  std::unique_ptr<MagmaWindowManager> window_manager_;
  std::unique_ptr<MagmaSurfaceFactory> surface_factory_;
  std::unique_ptr<PlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformMagma);
};

}  // namespace

OzonePlatform* CreateOzonePlatformMagma() {
  return new OzonePlatformMagma;
}

}  // namespace ui
