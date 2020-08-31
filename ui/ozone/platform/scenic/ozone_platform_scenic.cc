// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/ozone_platform_scenic.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ime/fuchsia/input_method_fuchsia.h"
#include "ui/display/fake/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/scenic/scenic_gpu_host.h"
#include "ui/ozone/platform/scenic/scenic_gpu_service.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/mojom/scenic_gpu_service.mojom.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

constexpr OzonePlatform::PlatformProperties kScenicPlatformProperties{
    /*needs_view_token=*/true,
    /*custom_frame_pref_default=*/false,
    /*use_system_title_bar=*/false,
    /*message_pump_type_for_gpu=*/base::MessagePumpType::IO,
    /*supports_vulkan_swap_chain=*/true,
};

class ScenicPlatformEventSource : public ui::PlatformEventSource {
 public:
  ScenicPlatformEventSource() = default;
  ~ScenicPlatformEventSource() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScenicPlatformEventSource);
};

// OzonePlatform for Scenic.
class OzonePlatformScenic
    : public OzonePlatform,
      public base::MessageLoopCurrent::DestructionObserver {
 public:
  OzonePlatformScenic() = default;
  ~OzonePlatformScenic() override = default;

  // OzonePlatform implementation.
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
    return scenic_gpu_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    BindInMainProcessIfNecessary();
    if (!properties.view_token.value) {
      NOTREACHED();
      return nullptr;
    }
    return std::make_unique<ScenicWindow>(window_manager_.get(), delegate,
                                          std::move(properties.view_token),
                                          std::move(properties.view_ref_pair));
  }

  const PlatformProperties& GetPlatformProperties() override {
    return kScenicPlatformProperties;
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    NOTIMPLEMENTED();
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return window_manager_->CreateScreen();
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<InputMethodFuchsia>(delegate, widget);
  }

  void InitializeUI(const InitParams& params) override {
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = std::make_unique<ScenicPlatformEventSource>();
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    window_manager_ = std::make_unique<ScenicWindowManager>();
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_ = std::make_unique<BitmapCursorFactoryOzone>();

    scenic_gpu_host_ = std::make_unique<ScenicGpuHost>(window_manager_.get());

    // SurfaceFactory is configured here to use a ui-process remote for software
    // output.
    if (!surface_factory_)
      surface_factory_ = std::make_unique<ScenicSurfaceFactory>();

    if (base::ThreadTaskRunnerHandle::IsSet())
      BindInMainProcessIfNecessary();
  }

  void InitializeGPU(const InitParams& params) override {
    DCHECK(!surface_factory_ || params.single_process);

    if (!surface_factory_)
      surface_factory_ = std::make_unique<ScenicSurfaceFactory>();

    if (!params.single_process) {
      mojo::PendingRemote<mojom::ScenicGpuHost> scenic_gpu_host_remote;
      scenic_gpu_service_ = std::make_unique<ScenicGpuService>(
          scenic_gpu_host_remote.InitWithNewPipeAndPassReceiver());

      // SurfaceFactory is configured here to use a gpu-process remote. The
      // other end of the pipe will be attached through ScenicGpuService.
      surface_factory_->Initialize(std::move(scenic_gpu_host_remote));
    }
  }

  void AddInterfaces(mojo::BinderMap* binders) override {
    binders->Add<mojom::ScenicGpuService>(
        scenic_gpu_service_->GetBinderCallback(),
        base::ThreadTaskRunnerHandle::Get());
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    return SysmemBufferCollection::IsNativePixmapConfigSupported(format, usage);
  }

 private:
  // Binds main process surface factory to main process ScenicGpuHost
  void BindInMainProcessIfNecessary() {
    if (bound_in_main_process_)
      return;

    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host_remote;
    scenic_gpu_host_->Initialize(
        gpu_host_remote.InitWithNewPipeAndPassReceiver());
    surface_factory_->Initialize(std::move(gpu_host_remote));
    bound_in_main_process_ = true;

    base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
  }

  void ShutdownInMainProcess() {
    DCHECK(bound_in_main_process_);
    surface_factory_->Shutdown();
    scenic_gpu_host_->Shutdown();
    window_manager_->Shutdown();
    bound_in_main_process_ = false;
  }

  // base::MessageLoopCurrent::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override { ShutdownInMainProcess(); }

  std::unique_ptr<ScenicWindowManager> window_manager_;

  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<PlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<ScenicGpuHost> scenic_gpu_host_;
  std::unique_ptr<ScenicGpuService> scenic_gpu_service_;
  std::unique_ptr<ScenicSurfaceFactory> surface_factory_;

  // Whether the main process has initialized mojo bindings.
  bool bound_in_main_process_ = false;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformScenic);
};

}  // namespace

OzonePlatform* CreateOzonePlatformScenic() {
  return new OzonePlatformScenic();
}

}  // namespace ui
