// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/ozone_platform_x11.h"

#include <memory>
#include <utility>

#include "base/message_loop/message_pump_type.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/buildflags.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/fake/fake_display_delegate.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/x11/gl_egl_utility_x11.h"
#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"
#include "ui/ozone/platform/x11/x11_cursor_factory_ozone.h"
#include "ui/ozone/platform/x11/x11_screen_ozone.h"
#include "ui/ozone/platform/x11/x11_surface_factory.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(OS_CHROMEOS)
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#else
#include "ui/base/ime/linux/input_method_auralinux.h"
#include "ui/ozone/platform/x11/x11_os_exchange_data_provider_ozone.h"
#endif

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_delegate.h"        // nogncheck
#include "ui/gtk/x/gtk_ui_delegate_x11.h"  // nogncheck
#endif

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

constexpr OzonePlatform::PlatformProperties kX11PlatformProperties{
    /*needs_view_token=*/false,
    /*custom_frame_pref_default=*/false,
    /*use_system_title_bar=*/true,

    // When the Ozone X11 backend is running, use a UI loop to grab Expose
    // events. See GLSurfaceGLX and https://crbug.com/326995.
    /*message_pump_type_for_gpu=*/base::MessagePumpType::UI,
    /*supports_vulkan_swap_chain=*/true,
};

// Singleton OzonePlatform implementation for X11 platform.
class OzonePlatformX11 : public OzonePlatform,
                         public ui::OSExchangeDataProviderFactoryOzone {
 public:
  OzonePlatformX11() { SetInstance(this); }

  ~OzonePlatformX11() override {}

  // OzonePlatform:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_ozone_.get();
  }

  ui::OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    std::unique_ptr<X11WindowOzone> window =
        std::make_unique<X11WindowOzone>(delegate);
    window->Initialize(std::move(properties));
    window->SetTitle(base::ASCIIToUTF16("Ozone X11"));
    return std::move(window);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    auto screen = std::make_unique<X11ScreenOzone>();
    screen->Init();
    return screen;
  }

  PlatformClipboard* GetPlatformClipboard() override {
    return clipboard_.get();
  }

  PlatformGLEGLUtility* GetPlatformGLEGLUtility() override {
    return gl_egl_utility_.get();
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      internal::InputMethodDelegate* delegate,
      gfx::AcceleratedWidget) override {
#if defined(OS_CHROMEOS)
    return std::make_unique<InputMethodChromeOS>(delegate);
#else
    // This method is used by upper layer components (e.g: GtkUi) to determine
    // if the LinuxInputMethodContextFactory instance is provided by the Ozone
    // platform implementation, so we must consider the case that it is still
    // not set at this point.
    if (!ui::LinuxInputMethodContextFactory::instance())
      return nullptr;
    return std::make_unique<InputMethodAuraLinux>(delegate);
#endif
  }

  std::unique_ptr<OSExchangeDataProvider> CreateProvider() override {
#if defined(OS_CHROMEOS)
    return std::make_unique<OSExchangeDataProviderAura>();
#else
    return std::make_unique<X11OSExchangeDataProviderOzone>();
#endif
  }

  const PlatformProperties& GetPlatformProperties() override {
    return kX11PlatformProperties;
  }

  void InitializeUI(const InitParams& params) override {
    InitializeCommon(params);
    CreatePlatformEventSource();
    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    clipboard_ = std::make_unique<X11ClipboardOzone>();
    cursor_factory_ozone_ = std::make_unique<X11CursorFactoryOzone>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

#if BUILDFLAG(USE_XKBCOMMON)
    keyboard_layout_engine_ =
        std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_);
    // TODO(nickdiego): debugging..
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
#else
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
#endif
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    TouchFactory::SetTouchDeviceListFromCommandLine();

#if BUILDFLAG(USE_GTK)
    DCHECK(!GtkUiDelegate::instance());
    gtk_ui_delegate_ = std::make_unique<GtkUiDelegateX11>(gfx::GetXDisplay());
    GtkUiDelegate::SetInstance(gtk_ui_delegate_.get());
#endif
  }

  void InitializeGPU(const InitParams& params) override {
    InitializeCommon(params);

    // In single process mode either the UI thread will create an event source
    // or it's a test and an event source isn't desired.
    if (!params.single_process)
      CreatePlatformEventSource();

    surface_factory_ozone_ = std::make_unique<X11SurfaceFactory>();
    gl_egl_utility_ = std::make_unique<GLEGLUtilityX11>();
  }

 private:
  // Performs initialization steps need by both UI and GPU.
  void InitializeCommon(const InitParams& params) {
    if (common_initialized_)
      return;

    // If XOpenDisplay() failed there is nothing we can do. Crash here instead
    // of crashing later. If you are crashing here, make sure there is an X
    // server running and $DISPLAY is set.
    CHECK(gfx::GetXDisplay()) << "Missing X server or $DISPLAY";

    ui::SetDefaultX11ErrorHandlers();

    common_initialized_ = true;
  }

  // Creates |event_source_| if it doesn't already exist.
  void CreatePlatformEventSource() {
    if (event_source_)
      return;

    XDisplay* display = gfx::GetXDisplay();
    event_source_ = std::make_unique<X11EventSource>(display);
  }

  bool common_initialized_ = false;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  // Objects in the UI process.
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<X11ClipboardOzone> clipboard_;
  std::unique_ptr<X11CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;

  // Objects in the GPU process.
  std::unique_ptr<X11SurfaceFactory> surface_factory_ozone_;
  std::unique_ptr<GLEGLUtilityX11> gl_egl_utility_;

  // Objects in both UI and GPU process.
  std::unique_ptr<X11EventSource> event_source_;

#if BUILDFLAG(USE_GTK)
  std::unique_ptr<GtkUiDelegate> gtk_ui_delegate_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformX11);
};

}  // namespace

OzonePlatform* CreateOzonePlatformX11() {
  return new OzonePlatformX11;
}

}  // namespace ui
