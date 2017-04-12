// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service_test_base.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/device_service.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

namespace device {

namespace {

const char kTestServiceName[] = "device_unittests";

// The test service responsible to package Device Service.
class ServiceTestClient : public service_manager::test::ServiceTestClient,
                          public service_manager::mojom::ServiceFactory,
                          public service_manager::InterfaceFactory<
                              service_manager::mojom::ServiceFactory> {
 public:
  explicit ServiceTestClient(service_manager::test::ServiceTest* test)
      : service_manager::test::ServiceTestClient(test),
        io_thread_("DeviceServiceTestIOThread"),
        file_thread_("DeviceServiceTestFileThread") {
    registry_.AddInterface<service_manager::mojom::ServiceFactory>(this);
  }
  ~ServiceTestClient() override {}

 protected:
  void OnBindInterface(const service_manager::ServiceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(source_info.identity, interface_name,
                            std::move(interface_pipe));
  }

  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == device::mojom::kServiceName) {
      io_thread_.Start();
      file_thread_.Start();
#if defined(OS_ANDROID)
      device_service_context_.reset(new service_manager::ServiceContext(
          CreateDeviceService(file_thread_.task_runner(),
                              io_thread_.task_runner(),
                              wake_lock_context_callback_),
          std::move(request)));
#else
      device_service_context_.reset(new service_manager::ServiceContext(
          CreateDeviceService(file_thread_.task_runner(),
                              io_thread_.task_runner()),
          std::move(request)));
#endif
    }
  }

  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ServiceFactoryRequest request) override {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

 private:
  base::Thread io_thread_;
  base::Thread file_thread_;
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;
  std::unique_ptr<service_manager::ServiceContext> device_service_context_;

  WakeLockContextCallback wake_lock_context_callback_;
};

}  // namespace

DeviceServiceTestBase::DeviceServiceTestBase()
    : ServiceTest(kTestServiceName) {}

DeviceServiceTestBase::~DeviceServiceTestBase() {}

std::unique_ptr<service_manager::Service>
DeviceServiceTestBase::CreateService() {
  return base::MakeUnique<ServiceTestClient>(this);
}

}  // namespace device
