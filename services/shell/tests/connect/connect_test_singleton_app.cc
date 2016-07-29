// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"

namespace shell {

class ConnectTestSingletonApp : public Service {
 public:
  ConnectTestSingletonApp() {}
  ~ConnectTestSingletonApp() override {}

 private:
  // shell::Service:
  bool OnConnect(Connection* connection) override {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(ConnectTestSingletonApp);
};

}  // namespace shell


MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new shell::ConnectTestSingletonApp);
  return runner.Run(service_request_handle);
}
