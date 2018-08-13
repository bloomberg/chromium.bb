// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/ui/gpu_host/gpu_host.h"
#include "services/ui/gpu_host/gpu_host_delegate.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/test_ws/test_drag_drop_client.h"
#include "services/ui/test_ws/test_gpu_interface_provider.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace ui {
namespace test {

// Service implementation that brings up the Window Service on top of aura.
// Uses ws2::WindowService to provide the Window Service and
// WindowTreeHostFactory to service requests for connections to the Window
// Service.
class TestWindowService : public service_manager::Service,
                          public service_manager::mojom::ServiceFactory,
                          public ui::gpu_host::GpuHostDelegate,
                          public ws2::WindowServiceDelegate {
 public:
  TestWindowService() = default;

  ~TestWindowService() override {
    aura::client::SetScreenPositionClient(aura_test_helper_->root_window(),
                                          nullptr);
    // WindowService depends upon Screen, which is owned by AuraTestHelper.
    service_context_.reset();
    // AuraTestHelper expects TearDown() to be called.
    aura_test_helper_->TearDown();
    aura_test_helper_.reset();
    ui::TerminateContextFactoryForTests();
  }

 private:
  // WindowServiceDelegate:
  std::unique_ptr<aura::Window> NewTopLevel(
      aura::PropertyConverter* property_converter,
      const base::flat_map<std::string, std::vector<uint8_t>>& properties)
      override {
    std::unique_ptr<aura::Window> top_level =
        std::make_unique<aura::Window>(nullptr);
    top_level->Init(LAYER_NOT_DRAWN);
    aura_test_helper_->root_window()->AddChild(top_level.get());
    for (auto property : properties) {
      property_converter->SetPropertyFromTransportValue(
          top_level.get(), property.first, &property.second);
    }
    return top_level;
  }
  void RunDragLoop(aura::Window* window,
                   const ui::OSExchangeData& data,
                   const gfx::Point& screen_location,
                   uint32_t drag_operation,
                   ui::DragDropTypes::DragEventSource source,
                   DragDropCompletedCallback callback) override {
    std::move(callback).Run(drag_drop_client_.StartDragAndDrop(
        data, window->GetRootWindow(), window, screen_location, drag_operation,
        source));
  }
  void CancelDragLoop(aura::Window* window) override {
    drag_drop_client_.DragCancel();
  }
  aura::WindowTreeHost* GetWindowTreeHostForDisplayId(
      int64_t display_id) override {
    return aura_test_helper_->host();
  }

  // service_manager::Service:
  void OnStart() override {
    CHECK(!started_);
    started_ = true;

    gfx::RegisterPathProvider();
    ui::RegisterPathProvider();

    registry_.AddInterface(base::BindRepeating(
        &TestWindowService::BindServiceFactory, base::Unretained(this)));

#if defined(OS_CHROMEOS)
    // Use gpu service only for ChromeOS to run content_browsertests in mash.
    //
    // To use this code path for all platforms, we need to fix the following
    // flaky failure on Win7 bot:
    //   gl_surface_egl.cc:
    //     EGL Driver message (Critical) eglInitialize: No available renderers
    //   gl_initializer_win.cc:
    //     GLSurfaceEGL::InitializeOneOff failed.
    CreateGpuHost();
#else
    gl::GLSurfaceTestSupport::InitializeOneOff();
    CreateAuraTestHelper();
#endif  // defined(OS_CHROMEOS)
  }
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    DCHECK_EQ(name, ui::mojom::kServiceName);

    // Defer CreateService if |aura_test_helper_| is not created.
    if (!aura_test_helper_) {
      DCHECK(!pending_create_service_);

      pending_create_service_ = base::BindOnce(
          &TestWindowService::CreateService, base::Unretained(this),
          std::move(request), name, std::move(pid_receiver));
      return;
    }

    DCHECK(!ui_service_created_);
    ui_service_created_ = true;

    auto window_service = std::make_unique<ws2::WindowService>(
        this,
        std::make_unique<TestGpuInterfaceProvider>(
            gpu_host_.get(), discardable_shared_memory_manager_.get()),
        aura_test_helper_->focus_client());
    service_context_ = std::make_unique<service_manager::ServiceContext>(
        std::move(window_service), std::move(request));
    pid_receiver->SetPID(base::GetCurrentProcId());
  }

  // ui::gpu_host::GpuHostDelegate:
  void OnGpuServiceInitialized() override {
    CreateAuraTestHelper();

    if (pending_create_service_)
      std::move(pending_create_service_).Run();
  }

  void BindServiceFactory(
      service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  void CreateGpuHost() {
    discardable_shared_memory_manager_ =
        std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();

    gpu_host_ = std::make_unique<ui::gpu_host::DefaultGpuHost>(
        this, context()->connector(), discardable_shared_memory_manager_.get());

    // |aura_test_helper_| is created later in OnGpuServiceInitialized.
  }

  void CreateAuraTestHelper() {
    DCHECK(!aura_test_helper_);

    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;
    ui::InitializeContextFactoryForTests(false /* enable_pixel_output */,
                                         &context_factory,
                                         &context_factory_private);
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
    aura_test_helper_->SetUp(context_factory, context_factory_private);

    aura::client::SetScreenPositionClient(aura_test_helper_->root_window(),
                                          &screen_position_client_);
  }

  service_manager::BinderRegistry registry_;

  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  // Handles the ServiceRequest. Owns the WindowService instance.
  std::unique_ptr<service_manager::ServiceContext> service_context_;

  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;

  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  std::unique_ptr<ui::gpu_host::DefaultGpuHost> gpu_host_;

  // For drag and drop code to convert to/from screen coordinates.
  wm::DefaultScreenPositionClient screen_position_client_;

  TestDragDropClient drag_drop_client_;

  bool started_ = false;
  bool ui_service_created_ = false;

  base::OnceClosure pending_create_service_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowService);
};

}  // namespace test
}  // namespace ui

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new ui::test::TestWindowService);
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(service_request_handle);
}
