// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "base/message_loop/message_loop.h"
#include "services/video_capture/service_impl.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::MessageLoop message_loop;
  video_capture::ServiceImpl(std::move(request), message_loop.task_runner())
      .RunUntilTermination();
}
