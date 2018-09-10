// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TEST_WS_TEST_WINDOW_SERVICE_H_
#define SERVICES_WS_TEST_WS_TEST_WINDOW_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/ws/gpu_host/gpu_host.h"
#include "services/ws/gpu_host/gpu_host_delegate.h"
#include "services/ws/test_ws/test_drag_drop_client.h"
#include "services/ws/window_service_delegate.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace ws {
namespace test {

// Service implementation that brings up the Window Service on top of aura.
// Uses ws::WindowService to provide the Window Service.
class TestWindowService : public service_manager::Service,
                          public service_manager::mojom::ServiceFactory,
                          public gpu_host::GpuHostDelegate,
                          public WindowServiceDelegate {
 public:
  TestWindowService();
  ~TestWindowService() override;

 private:
  // WindowServiceDelegate:
  std::unique_ptr<aura::Window> NewTopLevel(
      aura::PropertyConverter* property_converter,
      const base::flat_map<std::string, std::vector<uint8_t>>& properties)
      override;
  void RunDragLoop(aura::Window* window,
                   const ui::OSExchangeData& data,
                   const gfx::Point& screen_location,
                   uint32_t drag_operation,
                   ui::DragDropTypes::DragEventSource source,
                   DragDropCompletedCallback callback) override;
  void CancelDragLoop(aura::Window* window) override;
  aura::WindowTreeHost* GetWindowTreeHostForDisplayId(
      int64_t display_id) override;

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // service_manager::mojom::ServiceFactory:
  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override;

  // gpu_host::GpuHostDelegate:
  void OnGpuServiceInitialized() override;

  void BindServiceFactory(
      service_manager::mojom::ServiceFactoryRequest request);

  void CreateGpuHost();

  void CreateAuraTestHelper();

  service_manager::BinderRegistry registry_;

  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  // Handles the ServiceRequest. Owns the WindowService instance.
  std::unique_ptr<service_manager::ServiceContext> service_context_;

  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;

  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  std::unique_ptr<gpu_host::DefaultGpuHost> gpu_host_;

  // For drag and drop code to convert to/from screen coordinates.
  wm::DefaultScreenPositionClient screen_position_client_;

  TestDragDropClient drag_drop_client_;

  bool started_ = false;
  bool ui_service_created_ = false;

  base::OnceClosure pending_create_service_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowService);
};

}  // namespace test
}  // namespace ws

#endif  // SERVICES_WS_TEST_WS_TEST_WINDOW_SERVICE_H_
