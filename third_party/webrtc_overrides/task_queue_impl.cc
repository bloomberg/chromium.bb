// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace webrtc {

// Temporary link-time injection for the webrtc TaskQueueFactory implementation.
// This will not be needed once bugs.webrtc.org/10284 is resolved.
std::unique_ptr<TaskQueueFactory> CreateDefaultTaskQueueFactory() {
  return CreateWebRtcTaskQueueFactory();
}

}  // namespace webrtc
