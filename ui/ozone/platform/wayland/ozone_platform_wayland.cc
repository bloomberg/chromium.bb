// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/ozone_platform_wayland.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ime/linux/input_method_auralinux.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/linux/client_native_pixmap_dmabuf.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

#include "ui/gfx/buffer_format_util.h"

#if defined(WAYLAND_GBM)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/common/linux/gbm_wrapper.h"
#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"
#endif

namespace ui {

namespace {

constexpr OzonePlatform::PlatformProperties kWaylandPlatformProperties = {
    /*needs_view_token=*/false,

    // Supporting server-side decorations requires a support of
    // xdg-decorations. But this protocol has been accepted into the upstream
    // recently, and it will take time before it is taken by compositors. For
    // now, always use custom frames and disallow switching to server-side
    // frames.
    // https://github.com/wayland-project/wayland-protocols/commit/76d1ae8c65739eff3434ef219c58a913ad34e988
    /*custom_frame_pref_default=*/true,
    /*use_system_title_bar=*/false,

    // Ozone/Wayland relies on the mojo communication when running in
    // !single_process.
    // TODO(msisov, rjkroege): Remove after http://crbug.com/806092.
    /*requires_mojo=*/true,

    /*message_pump_type_for_gpu=*/base::MessagePumpType::DEFAULT};

class OzonePlatformWayland : public OzonePlatform {
 public:
  OzonePlatformWayland() {}
  ~OzonePlatformWayland() override {}

  // OzonePlatform
  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_.get();
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return buffer_manager_connector_ ? buffer_manager_connector_.get()
                                     : gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindowBase> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    auto window = std::make_unique<WaylandWindow>(delegate, connection_.get());
    if (!window->Initialize(std::move(properties)))
      return nullptr;
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return nullptr;
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    // The WaylandConnection and the WaylandOutputManager must be created
    // before PlatformScreen.
    DCHECK(connection_ && connection_->wayland_output_manager());
    return connection_->wayland_output_manager()->CreateWaylandScreen(
        connection_.get());
  }

  PlatformClipboard* GetPlatformClipboard() override {
    DCHECK(connection_);
    return connection_->clipboard();
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate) override {
    return std::make_unique<InputMethodAuraLinux>(delegate);
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    // If there is no drm render node device available, native pixmaps are not
    // supported.
    if (path_finder_.GetDrmRenderNodePath().empty())
      return false;

    if (supported_buffer_formats_.find(format) ==
        supported_buffer_formats_.end()) {
      return false;
    }

    return gfx::ClientNativePixmapDmaBuf::IsConfigurationSupported(format,
                                                                   usage);
  }

  void InitializeUI(const InitParams& args) override {
#if BUILDFLAG(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<StubKeyboardLayoutEngine>());
#endif
    connection_ = std::make_unique<WaylandConnection>();
    if (!connection_->Initialize())
      LOG(FATAL) << "Failed to initialize Wayland platform";

    buffer_manager_connector_ = std::make_unique<WaylandBufferManagerConnector>(
        connection_->buffer_manager_host());
    cursor_factory_ = std::make_unique<BitmapCursorFactoryOzone>();
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    supported_buffer_formats_ =
        connection_->buffer_manager_host()->GetSupportedBufferFormats();

    // Instantiate and set LinuxInputMethodContextFactory unless it is already
    // set (e.g: tests may have already set it).
    if (!LinuxInputMethodContextFactory::instance() &&
        !input_method_context_factory_) {
      input_method_context_factory_ =
          std::make_unique<WaylandInputMethodContextFactory>(connection_.get());
      LinuxInputMethodContextFactory::SetInstance(
          input_method_context_factory_.get());
    }
  }

  void InitializeGPU(const InitParams& args) override {
    buffer_manager_ = std::make_unique<WaylandBufferManagerGpu>();
    surface_factory_ = std::make_unique<WaylandSurfaceFactory>(
        connection_.get(), buffer_manager_.get());
#if defined(WAYLAND_GBM)
    const base::FilePath drm_node_path = path_finder_.GetDrmRenderNodePath();
    if (drm_node_path.empty()) {
      LOG(WARNING) << "Failed to find drm render node path.";
    } else {
      DrmRenderNodeHandle handle;
      if (!handle.Initialize(drm_node_path)) {
        LOG(WARNING) << "Failed to initialize drm render node handle.";
      } else {
        auto gbm = CreateGbmDevice(handle.PassFD().release());
        if (!gbm)
          LOG(WARNING) << "Failed to initialize gbm device.";
        buffer_manager_->set_gbm_device(std::move(gbm));
      }
    }
#endif
  }

  const PlatformProperties& GetPlatformProperties() override {
    return kWaylandPlatformProperties;
  }

  void AddInterfaces(service_manager::BinderRegistry* registry) override {
    registry->AddInterface<ozone::mojom::WaylandBufferManagerGpu>(
        base::BindRepeating(
            &OzonePlatformWayland::CreateWaylandBufferManagerGpuBinding,
            base::Unretained(this)));
  }

  void CreateWaylandBufferManagerGpuBinding(
      mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver) {
    buffer_manager_->AddBindingWaylandBufferManagerGpu(std::move(receiver));
  }

 private:
  std::unique_ptr<WaylandConnection> connection_;
  std::unique_ptr<WaylandSurfaceFactory> surface_factory_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_;
  std::unique_ptr<StubOverlayManager> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<WaylandInputMethodContextFactory>
      input_method_context_factory_;
  std::unique_ptr<WaylandBufferManagerConnector> buffer_manager_connector_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  // Objects, which solely live in the GPU process.
  std::unique_ptr<WaylandBufferManagerGpu> buffer_manager_;

  // Provides supported buffer formats for native gpu memory buffers
  // framework.
  wl::BufferFormatsWithModifiersMap supported_buffer_formats_;

  // This is used both in the gpu and browser processes to find out if a drm
  // render node is available.
  DrmRenderNodePathFinder path_finder_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformWayland);
};

}  // namespace

OzonePlatform* CreateOzonePlatformWayland() {
  return new OzonePlatformWayland;
}

}  // namespace ui
