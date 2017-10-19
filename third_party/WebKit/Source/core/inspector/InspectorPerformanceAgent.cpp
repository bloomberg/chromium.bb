// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorPerformanceAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/loader/DocumentLoader.h"
#include "core/paint/PaintTiming.h"
#include "core/probe/CoreProbes.h"
#include "platform/InstanceCounters.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/wtf/dtoa/utils.h"
#include "public/platform/Platform.h"

namespace blink {

using protocol::Response;

namespace {

static const char kPerformanceAgentEnabled[] = "PerformanceAgentEnabled";

constexpr bool isPlural(const char* str, int len) {
  return len > 1 && str[len - 2] == 's';
}

static constexpr const char* kInstanceCounterNames[] = {
#define INSTANCE_COUNTER_NAME(name) \
  (isPlural(#name, sizeof(#name)) ? #name : #name "s"),
    INSTANCE_COUNTERS_LIST(INSTANCE_COUNTER_NAME)
#undef INSTANCE_COUNTER_NAME
};

}  // namespace

InspectorPerformanceAgent::InspectorPerformanceAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorPerformanceAgent::~InspectorPerformanceAgent() = default;

void InspectorPerformanceAgent::Restore() {
  if (state_->booleanProperty(kPerformanceAgentEnabled, false))
    enable();
}

protocol::Response InspectorPerformanceAgent::enable() {
  if (enabled_)
    return Response::OK();
  enabled_ = true;
  state_->setBoolean(kPerformanceAgentEnabled, true);
  instrumenting_agents_->addInspectorPerformanceAgent(this);
  Platform::Current()->CurrentThread()->AddTaskTimeObserver(this);
  task_start_time_ = 0;
  script_start_time_ = 0;
  return Response::OK();
}

protocol::Response InspectorPerformanceAgent::disable() {
  if (!enabled_)
    return Response::OK();
  enabled_ = false;
  state_->setBoolean(kPerformanceAgentEnabled, false);
  instrumenting_agents_->removeInspectorPerformanceAgent(this);
  Platform::Current()->CurrentThread()->RemoveTaskTimeObserver(this);
  return Response::OK();
}

namespace {
void AppendMetric(protocol::Array<protocol::Performance::Metric>* container,
                  const String& name,
                  double value) {
  container->addItem(protocol::Performance::Metric::create()
                         .setName(name)
                         .setValue(value)
                         .build());
}
}  // namespace

Response InspectorPerformanceAgent::getMetrics(
    std::unique_ptr<protocol::Array<protocol::Performance::Metric>>*
        out_result) {
  if (!enabled_) {
    *out_result = protocol::Array<protocol::Performance::Metric>::create();
    return Response::OK();
  }

  std::unique_ptr<protocol::Array<protocol::Performance::Metric>> result =
      protocol::Array<protocol::Performance::Metric>::create();

  double now = (TimeTicks::Now() - TimeTicks()).InSecondsF();
  AppendMetric(result.get(), "Timestamp", now);

  // Renderer instance counters.
  for (size_t i = 0; i < ARRAY_SIZE(kInstanceCounterNames); ++i) {
    AppendMetric(result.get(), kInstanceCounterNames[i],
                 InstanceCounters::CounterValue(
                     static_cast<InstanceCounters::CounterType>(i)));
  }

  // Page performance metrics.
  AppendMetric(result.get(), "LayoutCount", static_cast<double>(layout_count_));
  AppendMetric(result.get(), "RecalcStyleCount",
               static_cast<double>(recalc_style_count_));
  AppendMetric(result.get(), "LayoutDuration", layout_duration_);
  AppendMetric(result.get(), "RecalcStyleDuration", recalc_style_duration_);
  double script_duration = script_duration_;
  if (script_start_time_)
    script_duration += now - script_start_time_;
  AppendMetric(result.get(), "ScriptDuration", script_duration);
  double task_duration = task_duration_;
  if (task_start_time_)
    task_duration += now - task_start_time_;
  AppendMetric(result.get(), "TaskDuration", task_duration);

  v8::HeapStatistics heap_statistics;
  V8PerIsolateData::MainThreadIsolate()->GetHeapStatistics(&heap_statistics);
  AppendMetric(result.get(), "JSHeapUsedSize",
               heap_statistics.used_heap_size());
  AppendMetric(result.get(), "JSHeapTotalSize",
               heap_statistics.total_heap_size());

  // Performance timings.
  Document* document = inspected_frames_->Root()->GetDocument();
  if (document) {
    AppendMetric(result.get(), "FirstMeaningfulPaint",
                 PaintTiming::From(*document).FirstMeaningfulPaint());
    AppendMetric(result.get(), "DomContentLoaded",
                 document->GetTiming().DomContentLoadedEventStart());
    AppendMetric(result.get(), "NavigationStart",
                 document->Loader()->GetTiming().NavigationStart());
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

void InspectorPerformanceAgent::Will(const probe::CallFunction& probe) {
  if (!script_call_depth_++)
    script_start_time_ = probe.CaptureStartTime();
}

void InspectorPerformanceAgent::Did(const probe::CallFunction& probe) {
  if (--script_call_depth_)
    return;
  script_duration_ += probe.Duration();
  script_start_time_ = 0;
}

void InspectorPerformanceAgent::Will(const probe::ExecuteScript& probe) {
  if (!script_call_depth_++)
    script_start_time_ = probe.CaptureStartTime();
}

void InspectorPerformanceAgent::Did(const probe::ExecuteScript& probe) {
  if (--script_call_depth_)
    return;
  script_duration_ += probe.Duration();
  script_start_time_ = 0;
}

void InspectorPerformanceAgent::Will(const probe::RecalculateStyle& probe) {
  probe.CaptureStartTime();
}

void InspectorPerformanceAgent::Did(const probe::RecalculateStyle& probe) {
  recalc_style_duration_ += probe.Duration();
  recalc_style_count_++;
}

void InspectorPerformanceAgent::Will(const probe::UpdateLayout& probe) {
  if (!layout_depth_++)
    probe.CaptureStartTime();
}

void InspectorPerformanceAgent::Did(const probe::UpdateLayout& probe) {
  if (--layout_depth_)
    return;
  layout_duration_ += probe.Duration();
  layout_count_++;
}

void InspectorPerformanceAgent::WillProcessTask(double start_time) {
  task_start_time_ = start_time;
}

void InspectorPerformanceAgent::DidProcessTask(double start_time,
                                               double end_time) {
  if (task_start_time_ == start_time)
    task_duration_ += end_time - start_time;
  task_start_time_ = 0;
}

void InspectorPerformanceAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent<protocol::Performance::Metainfo>::Trace(visitor);
}

}  // namespace blink
