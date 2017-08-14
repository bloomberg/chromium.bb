// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorPerformanceAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "platform/wtf/dtoa/utils.h"

namespace blink {

using protocol::Response;

const char* InspectorPerformanceAgent::metric_names_[] = {
#define DEFINE_PERF_METRIC_NAME(name) #name,
    PERF_METRICS_LIST(DEFINE_PERF_METRIC_NAME)
#undef DEFINE_PERF_METRIC_NAME
};

InspectorPerformanceAgent::InspectorPerformanceAgent(
    InspectedFrames* inspected_frames)
    : performance_monitor_(inspected_frames->Root()->GetPerformanceMonitor()) {}

InspectorPerformanceAgent::~InspectorPerformanceAgent() = default;

protocol::Response InspectorPerformanceAgent::enable() {
  enabled_ = true;
  instrumenting_agents_->addInspectorPerformanceAgent(this);
  return Response::OK();
}

protocol::Response InspectorPerformanceAgent::disable() {
  enabled_ = false;
  instrumenting_agents_->removeInspectorPerformanceAgent(this);
  return Response::OK();
}

Response InspectorPerformanceAgent::getMetrics(
    std::unique_ptr<protocol::Array<protocol::Performance::Metric>>*
        out_result) {
  std::unique_ptr<protocol::Array<protocol::Performance::Metric>> result =
      protocol::Array<protocol::Performance::Metric>::create();
  if (enabled_) {
    for (size_t i = 0; i < ARRAY_SIZE(metric_names_); ++i) {
      double value = performance_monitor_->PerfMetricValue(
          static_cast<PerformanceMonitor::MetricsType>(i));
      result->addItem(protocol::Performance::Metric::create()
                          .setName(metric_names_[i])
                          .setValue(value)
                          .build());
    }
  }
  *out_result = std::move(result);
  return Response::OK();
}

void InspectorPerformanceAgent::ConsoleTimeStamp(const String& title) {
  if (!enabled_)
    return;
  std::unique_ptr<protocol::Array<protocol::Performance::Metric>> metrics;
  getMetrics(&metrics);
  GetFrontend()->metrics(std::move(metrics), title);
}

DEFINE_TRACE(InspectorPerformanceAgent) {
  visitor->Trace(performance_monitor_);
  InspectorBaseAgent<protocol::Performance::Metainfo>::Trace(visitor);
}

}  // namespace blink
