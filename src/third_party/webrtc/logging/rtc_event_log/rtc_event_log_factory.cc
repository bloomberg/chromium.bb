/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_factory.h"

#include "absl/memory/memory.h"
#include "api/task_queue/global_task_queue_factory.h"

namespace webrtc {

std::unique_ptr<RtcEventLogFactoryInterface> CreateRtcEventLogFactory() {
  return absl::make_unique<RtcEventLogFactory>(&GlobalTaskQueueFactory());
}

}  // namespace webrtc
