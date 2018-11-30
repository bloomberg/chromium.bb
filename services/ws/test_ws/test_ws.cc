// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/standalone_service/service_main.h"
#include "services/ws/test_ws/test_window_service_factory.h"
#include "ui/base/ui_base_paths.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  ui::RegisterPathProvider();
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  auto service = ws::test::CreateOutOfProcessWindowService(std::move(request));
  service->RunUntilTermination();
}
