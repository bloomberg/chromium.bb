// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ime/test_ime_driver/test_ime_application.h"

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/ime/test_ime_driver/test_ime_driver.h"
#include "services/ui/public/interfaces/ime.mojom.h"

namespace ui {
namespace test {

TestIMEApplication::TestIMEApplication() {}

TestIMEApplication::~TestIMEApplication() {}

bool TestIMEApplication::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  return true;
}

void TestIMEApplication::OnStart(const service_manager::ServiceInfo& info) {
  mojom::IMEDriverPtr ime_driver_ptr;
  mojo::MakeStrongBinding(base::MakeUnique<TestIMEDriver>(),
                          GetProxy(&ime_driver_ptr));

  ui::mojom::IMERegistrarPtr ime_registrar;
  connector()->ConnectToInterface("service:ui", &ime_registrar);
  ime_registrar->RegisterDriver(std::move(ime_driver_ptr));
}

}  // namespace test
}  // namespace ui
