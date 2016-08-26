// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/ui/ime/test_ime_driver/test_ime_driver.h"

namespace ui {
namespace test {

class TestIME : public shell::Service {
 public:
  TestIME() {}
  ~TestIME() override {}

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {
    mojom::IMEDriverPtr ime_driver_ptr;
    new TestIMEDriver(GetProxy(&ime_driver_ptr));

    ui::mojom::IMERegistrarPtr ime_registrar;
    connector()->ConnectToInterface("mojo:ui", &ime_registrar);
    ime_registrar->RegisterDriver(std::move(ime_driver_ptr));
  }

  DISALLOW_COPY_AND_ASSIGN(TestIME);
};

}  // namespace test
}  // namespace ui

MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new ui::test::TestIME);
  return runner.Run(service_request_handle);
}
