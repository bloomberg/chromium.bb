// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/tests/lifecycle/app_client.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::test::AppClient* app = new shell::test::AppClient;
  shell::ServiceRunner runner(app);
  app->set_runner(&runner);
  return runner.Run(service_request_handle);
}
