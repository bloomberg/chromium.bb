// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/test_util.h"

#include <string>

#include "services/tracing/public/mojom/tracing.mojom.h"

namespace tracing {

MockAgent::MockAgent() : binding_(this) {}

MockAgent::~MockAgent() = default;

mojom::AgentPtr MockAgent::CreateAgentPtr() {
  mojom::AgentPtr agent_proxy;
  binding_.Bind(mojo::MakeRequest(&agent_proxy));
  return agent_proxy;
}

void MockAgent::StartTracing(const std::string& config,
                             base::TimeTicks coordinator_time,
                             const StartTracingCallback& cb) {
  call_stat_.push_back("StartTracing");
  cb.Run(true);
}

void MockAgent::StopAndFlush(mojom::RecorderPtr recorder) {
  call_stat_.push_back("StopAndFlush");
  if (!metadata_.empty())
    recorder->AddMetadata(metadata_.CreateDeepCopy());
  for (const auto& chunk : data_) {
    recorder->AddChunk(chunk);
  }
}

void MockAgent::RequestClockSyncMarker(
    const std::string& sync_id,
    const RequestClockSyncMarkerCallback& cb) {
  call_stat_.push_back("RequestClockSyncMarker");
}

void MockAgent::GetCategories(const GetCategoriesCallback& cb) {
  call_stat_.push_back("GetCategories");
  cb.Run(categories_);
}

void MockAgent::RequestBufferStatus(const RequestBufferStatusCallback& cb) {
  call_stat_.push_back("RequestBufferStatus");
  cb.Run(trace_log_status_.event_capacity, trace_log_status_.event_count);
}

}  // namespace tracing
