// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_tree_host_factory.mojom.h"
#include "services/ui/test_ws/test_gpu_interface_provider.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "services/ui/ws2/window_tree.h"
#include "services/ui/ws2/window_tree_binding.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ui {
namespace test {

class TestWS;

// See description in README.md for details on what this code is intended for.

// Holds the data for a single client connected via WindowTreeHostFactory. Each
// client has an aura::Window.
class Client {
 public:
  Client(ws2::WindowService* window_service,
         aura::Window* root,
         mojom::WindowTreeClientPtr tree_client_ptr) {
    window_ = std::make_unique<aura::Window>(nullptr);
    window_->Init(LAYER_NOT_DRAWN);
    window_->set_owned_by_parent(false);
    root->AddChild(window_.get());
    binding_ = std::make_unique<ws2::WindowTreeBinding>();
    mojom::WindowTreeClient* tree_client = tree_client_ptr.get();
    binding_->InitForEmbed(
        window_service, std::move(tree_client_ptr), tree_client, window_.get(),
        base::BindOnce(&Client::OnConnectionLost, base::Unretained(this)));
  }

  ~Client() = default;

 private:
  void OnConnectionLost() {
    binding_.reset();
    window_.reset();
  }

  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<ws2::WindowTreeBinding> binding_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

// mojom::WindowTreeHostFactory implementation that creates an instance of
// Client for all WindowTreeHost requests.
class WindowTreeHostFactory : public mojom::WindowTreeHostFactory {
 public:
  WindowTreeHostFactory(ws2::WindowService* window_service, aura::Window* root)
      : window_service_(window_service), root_(root) {}

  ~WindowTreeHostFactory() override = default;

  void AddBinding(mojom::WindowTreeHostFactoryRequest request) {
    window_tree_host_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  // mojom::WindowTreeHostFactory implementation.
  void CreateWindowTreeHost(mojom::WindowTreeHostRequest host,
                            mojom::WindowTreeClientPtr tree_client) override {
    // |host| is unused.
    clients_.push_back(std::make_unique<Client>(window_service_, root_,
                                                std::move(tree_client)));
  }

  ws2::WindowService* window_service_;
  aura::Window* root_;
  mojo::BindingSet<mojom::WindowTreeHostFactory>
      window_tree_host_factory_bindings_;
  std::vector<std::unique_ptr<Client>> clients_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostFactory);
};

// Service implementation that brings up the Window Service on top of aura.
// Uses ws2::WindowService to provide the Window Service and
// WindowTreeHostFactory to service requests for connections to the Window
// Service.
class TestWindowService : public service_manager::Service,
                          public service_manager::mojom::ServiceFactory,
                          public ws2::WindowServiceDelegate {
 public:
  TestWindowService() = default;

  ~TestWindowService() override {
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
    for (auto property : properties) {
      property_converter->SetPropertyFromTransportValue(
          top_level.get(), property.first, &property.second);
    }
    return top_level;
  }

  // service_manager::Service:
  void OnStart() override {
    CHECK(!started_);
    started_ = true;

    gl::GLSurfaceTestSupport::InitializeOneOff();
    gfx::RegisterPathProvider();
    ui::RegisterPathProvider();

    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;
    ui::InitializeContextFactoryForTests(false /* enable_pixel_output */,
                                         &context_factory,
                                         &context_factory_private);
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
    aura_test_helper_->SetUp(context_factory, context_factory_private);

    registry_.AddInterface(base::BindRepeating(
        &TestWindowService::BindServiceFactory, base::Unretained(this)));
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
    DCHECK(!ui_service_created_);
    ui_service_created_ = true;

    auto window_service = std::make_unique<ws2::WindowService>(
        this, std::make_unique<TestGpuInterfaceProvider>(),
        aura_test_helper_->focus_client());
    window_tree_host_factory_ = std::make_unique<WindowTreeHostFactory>(
        window_service.get(), aura_test_helper_->root_window());
    window_service->registry()->AddInterface(
        base::BindRepeating(&WindowTreeHostFactory::AddBinding,
                            base::Unretained(window_tree_host_factory_.get())));
    service_context_ = std::make_unique<service_manager::ServiceContext>(
        std::move(window_service), std::move(request));
    pid_receiver->SetPID(base::GetCurrentProcId());
  }

  void BindServiceFactory(
      service_manager::mojom::ServiceFactoryRequest request) {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  service_manager::BinderRegistry registry_;

  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  // Handles the ServiceRequest. Owns the WindowService instance.
  std::unique_ptr<service_manager::ServiceContext> service_context_;

  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<WindowTreeHostFactory> window_tree_host_factory_;

  bool started_ = false;
  bool ui_service_created_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWindowService);
};

}  // namespace test
}  // namespace ui

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new ui::test::TestWindowService);
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(service_request_handle);
}
