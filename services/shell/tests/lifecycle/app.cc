// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/tests/lifecycle/app_client.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::test::AppClient* app = new shell::test::AppClient;
  shell::ApplicationRunner runner(app);
  app->set_runner(&runner);
  return runner.Run(shell_handle);
}
