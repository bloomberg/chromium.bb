// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/ozone_platform_gbm.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdlib.h>
#include <xf86drm.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/ui_features.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/platform/drm/host/host_drm_device.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#else
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

class GlApiLoader {
 public:
  GlApiLoader()
      : glapi_lib_(dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL)) {}

  ~GlApiLoader() {
    if (glapi_lib_)
      dlclose(glapi_lib_);
  }

 private:
  // HACK: gbm drivers have broken linkage. The Mesa DRI driver references
  // symbols in the libglapi library however it does not explicitly link against
  // it. That caused linkage errors when running an application that does not
  // explicitly link against libglapi.
  void* glapi_lib_;

  DISALLOW_COPY_AND_ASSIGN(GlApiLoader);
};

class OzonePlatformGbm : public OzonePlatform {
 public:
  OzonePlatformGbm()
      : using_mojo_(false), single_process_(false), weak_factory_(this) {}
  ~OzonePlatformGbm() override {}

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
    return event_factory_ozone_->input_controller();
  }
  IPC::MessageFilter* GetGpuMessageFilter() override {
    return gpu_message_filter_.get();
  }
  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }
  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return event_factory_ozone_->CreateSystemInputInjector();
  }

  void AddInterfaces(
      service_manager::BinderRegistryWithArgs<
          const service_manager::BindSourceInfo&>* registry) override {
    registry->AddInterface<ozone::mojom::DeviceCursor>(
        base::Bind(&OzonePlatformGbm::CreateDeviceCursorBinding,
                   weak_factory_.GetWeakPtr()),
        gpu_task_runner_);

    registry->AddInterface<ozone::mojom::DrmDevice>(
        base::Bind(&OzonePlatformGbm::CreateDrmDeviceBinding,
                   weak_factory_.GetWeakPtr()),
        gpu_task_runner_);
  }
  void CreateDeviceCursorBinding(
      ozone::mojom::DeviceCursorRequest request,
      const service_manager::BindSourceInfo& source_info) {
    if (drm_thread_proxy_)
      drm_thread_proxy_->AddBindingCursorDevice(std::move(request));
    else
      pending_cursor_requests_.push_back(std::move(request));
  }

  // service_manager::InterfaceFactory<ozone::mojom::DrmDevice>:
  void CreateDrmDeviceBinding(
      ozone::mojom::DrmDeviceRequest request,
      const service_manager::BindSourceInfo& source_info) {
    if (drm_thread_proxy_)
      drm_thread_proxy_->AddBindingDrmDevice(std::move(request));
    else
      pending_gpu_adapter_requests_.push_back(std::move(request));
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override {
    GpuThreadAdapter* adapter = gpu_platform_support_host_.get();
    if (using_mojo_) {
      adapter = host_drm_device_.get();
    }

    std::unique_ptr<DrmWindowHost> platform_window(new DrmWindowHost(
        delegate, bounds, adapter, event_factory_ozone_.get(), cursor_.get(),
        window_manager_.get(), display_manager_.get(), overlay_manager_.get()));
    platform_window->Initialize();
    return std::move(platform_window);
  }
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return base::MakeUnique<DrmNativeDisplayDelegate>(display_manager_.get());
  }
  void InitializeUI(const InitParams& args) override {
    // Ozone drm can operate in three modes configured at runtime:
    //   1. legacy mode where host and viz components communicate
    //      via param traits IPC.
    //   2. single-process mode where host and viz components
    //      communicate via in-process mojo.
    //   3. multi-process mode where host and viz components communicate
    //      via mojo IPC.
    single_process_ = args.single_process;
    using_mojo_ = args.connector != nullptr;
    host_thread_ = base::PlatformThread::CurrentRef();

    DCHECK(!(using_mojo_ && !single_process_))
        << "Multiprocess Mojo is not supported yet.";

    device_manager_ = CreateDeviceManager();
    window_manager_.reset(new DrmWindowHostManager());
    cursor_.reset(new DrmCursor(window_manager_.get()));
#if BUILDFLAG(USE_XKBCOMMON)
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::MakeUnique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter_));
#else
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        base::MakeUnique<StubKeyboardLayoutEngine>());
#endif

    event_factory_ozone_.reset(new EventFactoryEvdev(
        cursor_.get(), device_manager_.get(),
        KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()));

    GpuThreadAdapter* adapter;

    if (single_process_)
      gl_api_loader_.reset(new GlApiLoader());

    if (using_mojo_) {
      host_drm_device_ =
          base::MakeUnique<HostDrmDevice>(cursor_.get(), args.connector);
      adapter = host_drm_device_.get();
    } else {
      gpu_platform_support_host_.reset(
          new DrmGpuPlatformSupportHost(cursor_.get()));
      adapter = gpu_platform_support_host_.get();
    }

    overlay_manager_.reset(
        new DrmOverlayManager(adapter, window_manager_.get()));
    display_manager_.reset(new DrmDisplayHostManager(
        adapter, device_manager_.get(), overlay_manager_.get(),
        event_factory_ozone_->input_controller()));
    cursor_factory_ozone_.reset(new BitmapCursorFactoryOzone);

    if (using_mojo_) {
      host_drm_device_->ProvideManagers(display_manager_.get(),
                                        overlay_manager_.get());
      host_drm_device_->AsyncStartDrmDevice();
    }
  }

  void InitializeGPU(const InitParams& args) override {
    // TODO(rjkroege): services/ui should initialize this with a connector.
    // However, in-progress refactorings in services/ui make it difficult to
    // require this at present. Set using_mojo_ like below once this is
    // complete.
    // TODO(rjk): Make it possible to turn this on.
    // using_mojo_ = args.connector != nullptr;
    gpu_task_runner_ = base::ThreadTaskRunnerHandle::Get();

    if (!single_process_)
      gl_api_loader_.reset(new GlApiLoader());

    InterThreadMessagingProxy* itmp;
    if (!using_mojo_) {
      scoped_refptr<DrmThreadMessageProxy> message_proxy(
          new DrmThreadMessageProxy());
      itmp = message_proxy.get();
      gpu_message_filter_ = std::move(message_proxy);
    }

    // NOTE: Can't start the thread here since this is called before sandbox
    // initialization in multi-process Chrome. In mus, we start the DRM thread.
    drm_thread_proxy_.reset(new DrmThreadProxy());

    surface_factory_.reset(new GbmSurfaceFactory(drm_thread_proxy_.get()));
    if (!using_mojo_) {
      drm_thread_proxy_->BindThreadIntoMessagingProxy(itmp);
    } else {
      drm_thread_proxy_->StartDrmThread();
    }

    // When the viz process (and hence the gpu portion of ozone/gbm) is
    // operating in a single process, the AddInterfaces method is best
    // invoked before the GPU thread launches.  As a result, requests to add
    // mojom bindings to the as yet un-launched service will fail so we queue
    // incoming binding requests until the GPU thread is running and play them
    // back here.
    for (auto& request : pending_cursor_requests_)
      drm_thread_proxy_->AddBindingCursorDevice(std::move(request));
    pending_cursor_requests_.clear();
    for (auto& request : pending_gpu_adapter_requests_)
      drm_thread_proxy_->AddBindingDrmDevice(std::move(request));
    pending_gpu_adapter_requests_.clear();

    // If InitializeGPU and InitializeUI are invoked on the same thread, startup
    // sequencing is complicated because tasks are queued on the unbound mojo
    // pipe connecting the UI (the host) to the DRM thread before the DRM thread
    // is launched above. Special case this sequence vis the
    // BlockingStartDrmDevice API.
    // TODO(rjkroege): In a future when we have completed splitting Viz, it will
    // be possible to simplify this logic.
    if (using_mojo_ && single_process_ &&
        host_thread_ == base::PlatformThread::CurrentRef()) {
      CHECK(host_drm_device_)
          << "Mojo single-process mode requires a HostDrmDevice.";
      host_drm_device_->BlockingStartDrmDevice();
    }
  }

 private:
  bool using_mojo_;
  bool single_process_;
  base::PlatformThreadRef host_thread_;

  // Objects in the GPU process.
  std::unique_ptr<DrmThreadProxy> drm_thread_proxy_;
  std::unique_ptr<GlApiLoader> gl_api_loader_;
  std::unique_ptr<GbmSurfaceFactory> surface_factory_;
  scoped_refptr<IPC::MessageFilter> gpu_message_filter_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // TODO(rjkroege,sadrul): Provide a more elegant solution for this issue when
  // running in single process mode.
  std::vector<ozone::mojom::DeviceCursorRequest> pending_cursor_requests_;
  std::vector<ozone::mojom::DrmDeviceRequest> pending_gpu_adapter_requests_;

  // gpu_platform_support_host_ is the IPC bridge to the GPU process while
  // host_drm_device_ is the mojo bridge to the Viz process. Only one can be in
  // use at any time.
  // TODO(rjkroege): Remove gpu_platform_support_host_ once ozone/drm with mojo
  // has reached the stable channel.
  // A raw pointer to either |gpu_platform_support_host_| or |host_drm_device_|
  // is passed to |display_manager_| and |overlay_manager_| in IntializeUI.
  // To avoid a use after free, the following two members should be declared
  // before the two managers, so that they're deleted after them.
  std::unique_ptr<DrmGpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<HostDrmDevice> host_drm_device_;

  // Objects in the Browser process.
  std::unique_ptr<DeviceManager> device_manager_;
  std::unique_ptr<BitmapCursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<DrmWindowHostManager> window_manager_;
  std::unique_ptr<DrmCursor> cursor_;
  std::unique_ptr<EventFactoryEvdev> event_factory_ozone_;
  std::unique_ptr<DrmDisplayHostManager> display_manager_;
  std::unique_ptr<DrmOverlayManager> overlay_manager_;

#if BUILDFLAG(USE_XKBCOMMON)
  XkbEvdevCodes xkb_evdev_code_converter_;
#endif

  base::WeakPtrFactory<OzonePlatformGbm> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformGbm);
};

}  // namespace

OzonePlatform* CreateOzonePlatformGbm() {
  return new OzonePlatformGbm;
}

}  // namespace ui
