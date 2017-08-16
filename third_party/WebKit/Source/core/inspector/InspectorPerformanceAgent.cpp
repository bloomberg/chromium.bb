// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorPerformanceAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/paint/PaintTiming.h"
#include "platform/InstanceCounters.h"
#include "platform/wtf/dtoa/utils.h"

namespace blink {

using protocol::Response;

InspectorPerformanceAgent::InspectorPerformanceAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

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

  // Renderer counters.
  AppendMetric(
      result.get(), "DocumentCount",
      InstanceCounters::CounterValue(InstanceCounters::kDocumentCounter));
  AppendMetric(result.get(), "FrameCount",
               InstanceCounters::CounterValue(InstanceCounters::kFrameCounter));
  AppendMetric(result.get(), "JSEventListenerCount",
               InstanceCounters::CounterValue(
                   InstanceCounters::kJSEventListenerCounter));
  AppendMetric(result.get(), "NodeCount",
               InstanceCounters::CounterValue(InstanceCounters::kNodeCounter));
  AppendMetric(
      result.get(), "ResourceCount",
      InstanceCounters::CounterValue(InstanceCounters::kResourceCounter));

  // Performance timings.
  Document* document = inspected_frames_->Root()->GetDocument();
  if (document) {
    const PaintTiming& paint_timing = PaintTiming::From(*document);
    AppendMetric(result.get(), "FirstMeaningfulPaint",
                 paint_timing.FirstMeaningfulPaint());
    const DocumentTiming& document_timing = document->GetTiming();
    AppendMetric(result.get(), "DomContentLoaded",
                 document_timing.DomContentLoadedEventStart());
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
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent<protocol::Performance::Metainfo>::Trace(visitor);
}

}  // namespace blink
