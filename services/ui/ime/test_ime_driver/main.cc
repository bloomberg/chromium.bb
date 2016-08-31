// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/ui/ime/test_ime_driver/test_ime_application.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new ui::test::TestIMEApplication);
  return runner.Run(service_request_handle);
}
