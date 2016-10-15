// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/tests/lifecycle/app_client.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::test::AppClient* app = new service_manager::test::AppClient;
  service_manager::ServiceRunner runner(app);
  app->set_runner(&runner);
  return runner.Run(service_request_handle);
}
