// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/ws/test_ws/test_window_service_factory.h"
#include "ui/base/ui_base_paths.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  ui::RegisterPathProvider();

  // |runner| takes ownership of the created test_ws service.
  service_manager::ServiceRunner runner(
      ws::test::CreateOutOfProcessWindowService().release());
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(service_request_handle);
}
