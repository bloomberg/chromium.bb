// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/background.test-mojom.h"

namespace service_manager {

// A service that exports a simple interface for testing. Used to test the
// parent background service manager.
class TestClient : public Service, public mojom::TestService {
 public:
  TestClient(mojom::ServiceRequest request)
      : service_binding_(this, std::move(request)) {
    registry_.AddInterface(base::BindRepeating(
        &TestClient::BindTestServiceRequest, base::Unretained(this)));
  }

  ~TestClient() override = default;

 private:
  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  // mojom::TestService
  void Test(TestCallback callback) override { std::move(callback).Run(); }

  void BindTestServiceRequest(mojom::TestServiceRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  void Quit() override { service_binding_.RequestClose(); }

  ServiceBinding service_binding_;
  BinderRegistry registry_;
  mojo::BindingSet<mojom::TestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace service_manager

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  service_manager::TestClient(std::move(request)).RunUntilTermination();
}
