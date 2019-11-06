// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "base/task/single_thread_task_executor.h"
#include "services/video_capture/service_impl.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_thread_task_executor;
  video_capture::ServiceImpl(std::move(request),
                             main_thread_task_executor.task_runner())
      .RunUntilTermination();
}
