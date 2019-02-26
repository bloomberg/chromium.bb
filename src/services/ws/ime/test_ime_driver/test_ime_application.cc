// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/ime/test_ime_driver/test_ime_application.h"

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/ime/test_ime_driver/test_ime_driver.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"

namespace ws {
namespace test {

TestIMEApplication::TestIMEApplication(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

TestIMEApplication::~TestIMEApplication() = default;

void TestIMEApplication::OnStart() {
  mojom::IMEDriverPtr ime_driver_ptr;
  mojo::MakeStrongBinding(std::make_unique<TestIMEDriver>(),
                          MakeRequest(&ime_driver_ptr));

  mojom::IMERegistrarPtr ime_registrar;
  service_binding_.GetConnector()->BindInterface(mojom::kServiceName,
                                                 &ime_registrar);
  ime_registrar->RegisterDriver(std::move(ime_driver_ptr));
}

}  // namespace test
}  // namespace ws
