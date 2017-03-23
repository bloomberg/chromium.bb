// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "bindings/core/v8/ScheduledAction.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/InspectorInstrumentationAgents.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace blink {

// static
double PerformanceMonitor::threshold(ExecutionContext* context,
                                     Violation violation) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::instrumentingMonitor(context);
  return monitor ? monitor->m_thresholds[violation] : 0;
}

// static
void PerformanceMonitor::reportGenericViolation(
    ExecutionContext* context,
    Violation violation,
    const String& text,
    double time,
    std::unique_ptr<SourceLocation> location) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (!monitor)
    return;
  monitor->innerReportGenericViolation(context, violation, text, time,
                                       std::move(location));
}

// static
PerformanceMonitor* PerformanceMonitor::monitor(
    const ExecutionContext* context) {
  if (!context || !context->isDocument())
    return nullptr;
  LocalFrame* frame = toDocument(context)->frame();
  if (!frame)
    return nullptr;
  return frame->performanceMonitor();
}

// static
PerformanceMonitor* PerformanceMonitor::instrumentingMonitor(
    const ExecutionContext* context) {
  PerformanceMonitor* monitor = PerformanceMonitor::monitor(context);
  return monitor && monitor->m_enabled ? monitor : nullptr;
}

PerformanceMonitor::PerformanceMonitor(LocalFrame* localRoot)
    : m_localRoot(localRoot) {
  std::fill(std::begin(m_thresholds), std::end(m_thresholds), 0);
  Platform::current()->currentThread()->addTaskTimeObserver(this);
  m_localRoot->instrumentingAgents()->addPerformanceMonitor(this);
}

PerformanceMonitor::~PerformanceMonitor() {
  DCHECK(!m_localRoot);
}

void PerformanceMonitor::subscribe(Violation violation,
                                   double threshold,
                                   Client* client) {
  DCHECK(violation < kAfterLast);
  ClientThresholds* clientThresholds = m_subscriptions.at(violation);
  if (!clientThresholds) {
    clientThresholds = new ClientThresholds();
    m_subscriptions.set(violation, clientThresholds);
  }
  clientThresholds->set(client, threshold);
  updateInstrumentation();
}

void PerformanceMonitor::unsubscribeAll(Client* client) {
  for (const auto& it : m_subscriptions)
    it.value->erase(client);
  updateInstrumentation();
}

void PerformanceMonitor::shutdown() {
  if (!m_localRoot)
    return;
  m_subscriptions.clear();
  updateInstrumentation();
  Platform::current()->currentThread()->removeTaskTimeObserver(this);
  m_localRoot->instrumentingAgents()->removePerformanceMonitor(this);
  m_localRoot = nullptr;
}

void PerformanceMonitor::updateInstrumentation() {
  std::fill(std::begin(m_thresholds), std::end(m_thresholds), 0);

  for (const auto& it : m_subscriptions) {
    Violation violation = static_cast<Violation>(it.key);
    ClientThresholds* clientThresholds = it.value;
    for (const auto& clientThreshold : *clientThresholds) {
      if (!m_thresholds[violation] ||
          m_thresholds[violation] > clientThreshold.value)
        m_thresholds[violation] = clientThreshold.value;
    }
  }

  m_enabled = std::count(std::begin(m_thresholds), std::end(m_thresholds), 0) <
              static_cast<int>(kAfterLast);
}

void PerformanceMonitor::willExecuteScript(ExecutionContext* context) {
  // Heuristic for minimal frame context attribution: note the frame context
  // for each script execution. When a long task is encountered,
  // if there is only one frame context involved, then report it.
  // Otherwise don't report frame context.
  // NOTE: This heuristic is imperfect and will be improved in V2 API.
  // In V2, timing of script execution along with style & layout updates will be
  // accounted for detailed and more accurate attribution.
  ++m_scriptDepth;
  if (!m_taskExecutionContext)
    m_taskExecutionContext = context;
  else if (m_taskExecutionContext != context)
    m_taskHasMultipleContexts = true;
}

void PerformanceMonitor::didExecuteScript() {
  --m_scriptDepth;
}

void PerformanceMonitor::will(const probe::RecalculateStyle& probe) {
  if (m_enabled && m_thresholds[kLongLayout] && m_scriptDepth)
    probe.captureStartTime();
}

void PerformanceMonitor::did(const probe::RecalculateStyle& probe) {
  if (m_enabled && m_scriptDepth && m_thresholds[kLongLayout])
    m_perTaskStyleAndLayoutTime += probe.duration();
}

void PerformanceMonitor::will(const probe::UpdateLayout& probe) {
  ++m_layoutDepth;
  if (!m_enabled)
    return;
  if (m_layoutDepth > 1 || !m_scriptDepth || !m_thresholds[kLongLayout])
    return;

  probe.captureStartTime();
}

void PerformanceMonitor::did(const probe::UpdateLayout& probe) {
  --m_layoutDepth;
  if (!m_enabled)
    return;
  if (m_thresholds[kLongLayout] && m_scriptDepth && !m_layoutDepth)
    m_perTaskStyleAndLayoutTime += probe.duration();
}

void PerformanceMonitor::will(const probe::ExecuteScript& probe) {
  willExecuteScript(probe.context);
}

void PerformanceMonitor::did(const probe::ExecuteScript& probe) {
  didExecuteScript();
}

void PerformanceMonitor::will(const probe::CallFunction& probe) {
  willExecuteScript(probe.context);
  if (m_userCallback)
    probe.captureStartTime();
}

void PerformanceMonitor::did(const probe::CallFunction& probe) {
  didExecuteScript();
  if (!m_enabled || !m_userCallback)
    return;

  // Working around Oilpan - probes are STACK_ALLOCATED.
  const probe::UserCallback* userCallback =
      static_cast<const probe::UserCallback*>(m_userCallback);
  Violation handlerType =
      userCallback->recurring ? kRecurringHandler : kHandler;
  double threshold = m_thresholds[handlerType];
  double duration = probe.duration();
  if (!threshold || duration < threshold)
    return;

  String name = userCallback->name ? String(userCallback->name)
                                   : String(userCallback->atomicName);
  String text = String::format("'%s' handler took %ldms", name.utf8().data(),
                               lround(duration * 1000));
  innerReportGenericViolation(probe.context, handlerType, text, duration,
                              SourceLocation::fromFunction(probe.function));
}

void PerformanceMonitor::will(const probe::UserCallback& probe) {
  ++m_userCallbackDepth;

  if (!m_enabled || m_userCallbackDepth != 1 ||
      !m_thresholds[probe.recurring ? kRecurringHandler : kHandler])
    return;

  DCHECK(!m_userCallback);
  m_userCallback = &probe;
}

void PerformanceMonitor::did(const probe::UserCallback& probe) {
  --m_userCallbackDepth;
  if (!m_userCallbackDepth)
    m_userCallback = nullptr;
}

void PerformanceMonitor::documentWriteFetchScript(Document* document) {
  if (!m_enabled)
    return;
  String text = "Parser was blocked due to document.write(<script>)";
  innerReportGenericViolation(document, kBlockedParser, text, 0, nullptr);
}

void PerformanceMonitor::willProcessTask(scheduler::TaskQueue*,
                                         double startTime) {
  // Reset m_taskExecutionContext. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  m_taskExecutionContext = nullptr;
  m_taskHasMultipleContexts = false;

  if (!m_enabled)
    return;

  // Reset everything for regular and nested tasks.
  m_scriptDepth = 0;
  m_layoutDepth = 0;
  m_perTaskStyleAndLayoutTime = 0;
  m_userCallback = nullptr;
}

void PerformanceMonitor::didProcessTask(scheduler::TaskQueue*,
                                        double startTime,
                                        double endTime) {
  if (!m_enabled)
    return;
  double layoutThreshold = m_thresholds[kLongLayout];
  if (layoutThreshold && m_perTaskStyleAndLayoutTime > layoutThreshold) {
    ClientThresholds* clientThresholds = m_subscriptions.at(kLongLayout);
    DCHECK(clientThresholds);
    for (const auto& it : *clientThresholds) {
      if (it.value < m_perTaskStyleAndLayoutTime)
        it.key->reportLongLayout(m_perTaskStyleAndLayoutTime);
    }
  }

  double taskTime = endTime - startTime;
  if (m_thresholds[kLongTask] && taskTime > m_thresholds[kLongTask]) {
    ClientThresholds* clientThresholds = m_subscriptions.at(kLongTask);
    for (const auto& it : *clientThresholds) {
      if (it.value < taskTime) {
        it.key->reportLongTask(
            startTime, endTime,
            m_taskHasMultipleContexts ? nullptr : m_taskExecutionContext,
            m_taskHasMultipleContexts);
      }
    }
  }
}

void PerformanceMonitor::innerReportGenericViolation(
    ExecutionContext* context,
    Violation violation,
    const String& text,
    double time,
    std::unique_ptr<SourceLocation> location) {
  ClientThresholds* clientThresholds = m_subscriptions.at(violation);
  if (!clientThresholds)
    return;
  if (!location)
    location = SourceLocation::capture(context);
  for (const auto& it : *clientThresholds) {
    if (it.value < time) {
      if (!location)
        location = SourceLocation::capture(context);
      it.key->reportGenericViolation(violation, text, time, location.get());
    }
  }
}

DEFINE_TRACE(PerformanceMonitor) {
  visitor->trace(m_localRoot);
  visitor->trace(m_taskExecutionContext);
  visitor->trace(m_subscriptions);
}

}  // namespace blink
