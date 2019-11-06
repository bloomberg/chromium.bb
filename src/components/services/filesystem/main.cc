// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"
#include "components/services/filesystem/file_system_app.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_thread_task_executor;
  filesystem::FileSystemApp(std::move(request)).RunUntilTermination();
}
