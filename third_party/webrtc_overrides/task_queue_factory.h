// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_TASK_QUEUE_FACTORY_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_TASK_QUEUE_FACTORY_H_

#include <memory>

#include "base/task/task_traits.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"

// Creates factory for webrtc::TaskQueueBase backed by base::SequencedTaskRunner
// Tested by /content/renderer/media/webrtc/task_queue_factory_unittest.cc
std::unique_ptr<webrtc::TaskQueueFactory> CreateWebRtcTaskQueueFactory();

std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
CreateWebRtcTaskQueue(webrtc::TaskQueueFactory::Priority priority);

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_TASK_QUEUE_FACTORY_H_
