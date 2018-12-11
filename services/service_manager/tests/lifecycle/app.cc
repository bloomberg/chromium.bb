// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/tests/lifecycle/app_client.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  service_manager::test::AppClient(std::move(request)).RunUntilTermination();
}
