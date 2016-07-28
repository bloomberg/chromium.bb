// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/ui/test_ime/test_ime_driver.h"

namespace ui {
namespace test {

class TestIME : public shell::Service,
                public shell::InterfaceFactory<mojom::IMEDriver> {
 public:
  TestIME() {}
  ~TestIME() override {}

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {}
  bool OnConnect(shell::Connection* connection) override {
    connection->AddInterface<mojom::IMEDriver>(this);
    return true;
  }

  // shell::InterfaceFactory<mojom::IMEDriver>:
  void Create(const shell::Identity& remote_identity,
              mojom::IMEDriverRequest request) override {
    ime_driver_.reset(new TestIMEDriver(std::move(request)));
  }

  std::unique_ptr<TestIMEDriver> ime_driver_;

  DISALLOW_COPY_AND_ASSIGN(TestIME);
};

}  // namespace test
}  // namespace ui

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ServiceRunner runner(new ui::test::TestIME);
  return runner.Run(shell_handle);
}
