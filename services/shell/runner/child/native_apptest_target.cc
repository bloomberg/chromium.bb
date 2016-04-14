// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/runner/child/test_native_main.h"
#include "services/shell/runner/child/test_native_service.mojom.h"
#include "services/shell/runner/init.h"

namespace {

class TargetApplicationDelegate
    : public shell::ShellClient,
      public shell::test::TestNativeService,
      public shell::InterfaceFactory<shell::test::TestNativeService> {
 public:
  TargetApplicationDelegate() {}
  ~TargetApplicationDelegate() override {}

 private:
  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<shell::test::TestNativeService>(this);
    return true;
  }

  // shell::test::TestNativeService:
  void Invert(bool from_driver, const InvertCallback& callback) override {
    callback.Run(!from_driver);
  }

  // shell::InterfaceFactory<shell::test::TestNativeService>:
  void Create(
      shell::Connection* connection,
      mojo::InterfaceRequest<shell::test::TestNativeService> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::BindingSet<shell::test::TestNativeService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TargetApplicationDelegate);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  shell::InitializeLogging();

  TargetApplicationDelegate delegate;
  return shell::TestNativeMain(&delegate);
}
