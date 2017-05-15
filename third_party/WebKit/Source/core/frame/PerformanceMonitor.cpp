// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "bindings/core/v8/ScheduledAction.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreProbeSink.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/probe/CoreProbes.h"
#include "platform/wtf/CurrentTime.h"
#include "public/platform/Platform.h"

namespace blink {

// static
double PerformanceMonitor::Threshold(ExecutionContext* context,
                                     Violation violation) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::InstrumentingMonitor(context);
  return monitor ? monitor->thresholds_[violation] : 0;
}

// static
void PerformanceMonitor::ReportGenericViolation(
    ExecutionContext* context,
    Violation violation,
    const String& text,
    double time,
    std::unique_ptr<SourceLocation> location) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::InstrumentingMonitor(context);
  if (!monitor)
    return;
  monitor->InnerReportGenericViolation(context, violation, text, time,
                                       std::move(location));
}

// static
PerformanceMonitor* PerformanceMonitor::Monitor(
    const ExecutionContext* context) {
  if (!context || !context->IsDocument())
    return nullptr;
  LocalFrame* frame = ToDocument(context)->GetFrame();
  if (!frame)
    return nullptr;
  return frame->GetPerformanceMonitor();
}

// static
PerformanceMonitor* PerformanceMonitor::InstrumentingMonitor(
    const ExecutionContext* context) {
  PerformanceMonitor* monitor = PerformanceMonitor::Monitor(context);
  return monitor && monitor->enabled_ ? monitor : nullptr;
}

PerformanceMonitor::PerformanceMonitor(LocalFrame* local_root)
    : local_root_(local_root) {
  std::fill(std::begin(thresholds_), std::end(thresholds_), 0);
  Platform::Current()->CurrentThread()->AddTaskTimeObserver(this);
  local_root_->GetProbeSink()->addPerformanceMonitor(this);
}

PerformanceMonitor::~PerformanceMonitor() {
  DCHECK(!local_root_);
}

void PerformanceMonitor::Subscribe(Violation violation,
                                   double threshold,
                                   Client* client) {
  DCHECK(violation < kAfterLast);
  ClientThresholds* client_thresholds = subscriptions_.at(violation);
  if (!client_thresholds) {
    client_thresholds = new ClientThresholds();
    subscriptions_.Set(violation, client_thresholds);
  }
  client_thresholds->Set(client, threshold);
  UpdateInstrumentation();
}

void PerformanceMonitor::UnsubscribeAll(Client* client) {
  for (const auto& it : subscriptions_)
    it.value->erase(client);
  UpdateInstrumentation();
}

void PerformanceMonitor::Shutdown() {
  if (!local_root_)
    return;
  subscriptions_.clear();
  UpdateInstrumentation();
  Platform::Current()->CurrentThread()->RemoveTaskTimeObserver(this);
  local_root_->GetProbeSink()->removePerformanceMonitor(this);
  local_root_ = nullptr;
}

void PerformanceMonitor::UpdateInstrumentation() {
  std::fill(std::begin(thresholds_), std::end(thresholds_), 0);

  for (const auto& it : subscriptions_) {
    Violation violation = static_cast<Violation>(it.key);
    ClientThresholds* client_thresholds = it.value;
    for (const auto& client_threshold : *client_thresholds) {
      if (!thresholds_[violation] ||
          thresholds_[violation] > client_threshold.value)
        thresholds_[violation] = client_threshold.value;
    }
  }

  enabled_ = std::count(std::begin(thresholds_), std::end(thresholds_), 0) <
             static_cast<int>(kAfterLast);
}

void PerformanceMonitor::WillExecuteScript(ExecutionContext* context) {
  // Heuristic for minimal frame context attribution: note the frame context
  // for each script execution. When a long task is encountered,
  // if there is only one frame context involved, then report it.
  // Otherwise don't report frame context.
  // NOTE: This heuristic is imperfect and will be improved in V2 API.
  // In V2, timing of script execution along with style & layout updates will be
  // accounted for detailed and more accurate attribution.
  ++script_depth_;
  if (!task_execution_context_)
    task_execution_context_ = context;
  else if (task_execution_context_ != context)
    task_has_multiple_contexts_ = true;
}

void PerformanceMonitor::DidExecuteScript() {
  --script_depth_;
}

void PerformanceMonitor::Will(const probe::RecalculateStyle& probe) {
  if (enabled_ && thresholds_[kLongLayout] && script_depth_)
    probe.CaptureStartTime();
}

void PerformanceMonitor::Did(const probe::RecalculateStyle& probe) {
  if (enabled_ && script_depth_ && thresholds_[kLongLayout])
    per_task_style_and_layout_time_ += probe.Duration();
}

void PerformanceMonitor::Will(const probe::UpdateLayout& probe) {
  ++layout_depth_;
  if (!enabled_)
    return;
  if (layout_depth_ > 1 || !script_depth_ || !thresholds_[kLongLayout])
    return;

  probe.CaptureStartTime();
}

void PerformanceMonitor::Did(const probe::UpdateLayout& probe) {
  --layout_depth_;
  if (!enabled_)
    return;
  if (thresholds_[kLongLayout] && script_depth_ && !layout_depth_)
    per_task_style_and_layout_time_ += probe.Duration();
}

void PerformanceMonitor::Will(const probe::ExecuteScript& probe) {
  WillExecuteScript(probe.context);
}

void PerformanceMonitor::Did(const probe::ExecuteScript& probe) {
  DidExecuteScript();
}

void PerformanceMonitor::Will(const probe::CallFunction& probe) {
  WillExecuteScript(probe.context);
  if (user_callback_)
    probe.CaptureStartTime();
}

void PerformanceMonitor::Did(const probe::CallFunction& probe) {
  DidExecuteScript();
  if (!enabled_ || !user_callback_)
    return;

  // Working around Oilpan - probes are STACK_ALLOCATED.
  const probe::UserCallback* user_callback =
      static_cast<const probe::UserCallback*>(user_callback_);
  Violation handler_type =
      user_callback->recurring ? kRecurringHandler : kHandler;
  double threshold = thresholds_[handler_type];
  double duration = probe.Duration();
  if (!threshold || duration < threshold)
    return;

  String name = user_callback->name ? String(user_callback->name)
                                    : String(user_callback->atomicName);
  String text = String::Format("'%s' handler took %ldms", name.Utf8().data(),
                               lround(duration * 1000));
  InnerReportGenericViolation(probe.context, handler_type, text, duration,
                              SourceLocation::FromFunction(probe.function));
}

void PerformanceMonitor::Will(const probe::UserCallback& probe) {
  ++user_callback_depth_;

  if (!enabled_ || user_callback_depth_ != 1 ||
      !thresholds_[probe.recurring ? kRecurringHandler : kHandler])
    return;

  DCHECK(!user_callback_);
  user_callback_ = &probe;
}

void PerformanceMonitor::Did(const probe::UserCallback& probe) {
  --user_callback_depth_;
  if (!user_callback_depth_)
    user_callback_ = nullptr;
}

void PerformanceMonitor::DocumentWriteFetchScript(Document* document) {
  if (!enabled_)
    return;
  String text = "Parser was blocked due to document.write(<script>)";
  InnerReportGenericViolation(document, kBlockedParser, text, 0, nullptr);
}

void PerformanceMonitor::WillProcessTask(scheduler::TaskQueue*,
                                         double start_time) {
  // Reset m_taskExecutionContext. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  task_execution_context_ = nullptr;
  task_has_multiple_contexts_ = false;

  if (!enabled_)
    return;

  // Reset everything for regular and nested tasks.
  script_depth_ = 0;
  layout_depth_ = 0;
  per_task_style_and_layout_time_ = 0;
  user_callback_ = nullptr;
}

void PerformanceMonitor::DidProcessTask(scheduler::TaskQueue*,
                                        double start_time,
                                        double end_time) {
  if (!enabled_)
    return;
  double layout_threshold = thresholds_[kLongLayout];
  if (layout_threshold && per_task_style_and_layout_time_ > layout_threshold) {
    ClientThresholds* client_thresholds = subscriptions_.at(kLongLayout);
    DCHECK(client_thresholds);
    for (const auto& it : *client_thresholds) {
      if (it.value < per_task_style_and_layout_time_)
        it.key->ReportLongLayout(per_task_style_and_layout_time_);
    }
  }

  double task_time = end_time - start_time;
  if (thresholds_[kLongTask] && task_time > thresholds_[kLongTask]) {
    ClientThresholds* client_thresholds = subscriptions_.at(kLongTask);
    for (const auto& it : *client_thresholds) {
      if (it.value < task_time) {
        it.key->ReportLongTask(
            start_time, end_time,
            task_has_multiple_contexts_ ? nullptr : task_execution_context_,
            task_has_multiple_contexts_);
      }
    }
  }
}

void PerformanceMonitor::InnerReportGenericViolation(
    ExecutionContext* context,
    Violation violation,
    const String& text,
    double time,
    std::unique_ptr<SourceLocation> location) {
  ClientThresholds* client_thresholds = subscriptions_.at(violation);
  if (!client_thresholds)
    return;
  if (!location)
    location = SourceLocation::Capture(context);
  for (const auto& it : *client_thresholds) {
    if (it.value < time)
      it.key->ReportGenericViolation(violation, text, time, location.get());
  }
}

DEFINE_TRACE(PerformanceMonitor) {
  visitor->Trace(local_root_);
  visitor->Trace(task_execution_context_);
  visitor->Trace(subscriptions_);
}

}  // namespace blink
