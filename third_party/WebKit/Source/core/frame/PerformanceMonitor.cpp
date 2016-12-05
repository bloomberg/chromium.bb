// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "bindings/core/v8/ScheduledAction.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventListener.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace blink {

PerformanceMonitor::HandlerCall::HandlerCall(ExecutionContext* context,
                                             bool recurring)
    : m_performanceMonitor(PerformanceMonitor::instrumentingMonitor(context)) {
  if (!m_performanceMonitor)
    return;
  Violation violation = recurring ? kRecurringHandler : kHandler;
  if (!m_performanceMonitor->m_thresholds[violation]) {
    m_performanceMonitor = nullptr;
    return;
  }
  if (!m_performanceMonitor->m_handlerDepth)
    m_performanceMonitor->m_handlerType = violation;
  ++m_performanceMonitor->m_handlerDepth;
}

PerformanceMonitor::HandlerCall::HandlerCall(ExecutionContext* context,
                                             const char* name,
                                             bool recurring)
    : HandlerCall(context, recurring) {
  if (m_performanceMonitor && m_performanceMonitor->m_handlerDepth == 1)
    m_performanceMonitor->m_handlerName = name;
}

PerformanceMonitor::HandlerCall::HandlerCall(ExecutionContext* context,
                                             const AtomicString& name,
                                             bool recurring)
    : HandlerCall(context, recurring) {
  if (m_performanceMonitor && m_performanceMonitor->m_handlerDepth == 1)
    m_performanceMonitor->m_handlerAtomicName = name;
}

PerformanceMonitor::HandlerCall::~HandlerCall() {
  if (!m_performanceMonitor)
    return;
  --m_performanceMonitor->m_handlerDepth;
  if (!m_performanceMonitor->m_handlerDepth) {
    m_performanceMonitor->m_handlerType = PerformanceMonitor::kAfterLast;
    m_performanceMonitor->m_handlerName = nullptr;
    m_performanceMonitor->m_handlerAtomicName = AtomicString();
  }
}

// static
void PerformanceMonitor::willExecuteScript(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor = PerformanceMonitor::monitor(context);
  if (performanceMonitor)
    performanceMonitor->alwaysWillExecuteScript(context);
}

// static
void PerformanceMonitor::didExecuteScript(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor = PerformanceMonitor::monitor(context);
  if (performanceMonitor)
    performanceMonitor->alwaysDidExecuteScript();
}

// static
void PerformanceMonitor::willCallFunction(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor = PerformanceMonitor::monitor(context);
  if (performanceMonitor)
    performanceMonitor->alwaysWillCallFunction(context);
}

// static
void PerformanceMonitor::didCallFunction(ExecutionContext* context,
                                         v8::Local<v8::Function> function) {
  PerformanceMonitor* performanceMonitor = PerformanceMonitor::monitor(context);
  if (performanceMonitor)
    performanceMonitor->alwaysDidCallFunction(function);
}

// static
void PerformanceMonitor::willUpdateLayout(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->willUpdateLayout();
}

// static
void PerformanceMonitor::didUpdateLayout(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->didUpdateLayout();
}

// static
void PerformanceMonitor::willRecalculateStyle(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->willRecalculateStyle();
}

// static
void PerformanceMonitor::didRecalculateStyle(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (performanceMonitor)
    performanceMonitor->didRecalculateStyle();
}

// static
void PerformanceMonitor::documentWriteFetchScript(Document* document) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(document);
  if (!performanceMonitor)
    return;
  String text = "Parser was blocked due to document.write(<script>)";
  performanceMonitor->reportGenericViolation(
      kBlockedParser, text, 0, SourceLocation::capture(document).get());
}

// static
double PerformanceMonitor::threshold(ExecutionContext* context,
                                     Violation violation) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::instrumentingMonitor(context);
  return monitor ? monitor->m_thresholds[violation] : 0;
}

// static
void PerformanceMonitor::reportGenericViolation(ExecutionContext* context,
                                                Violation violation,
                                                const String& text,
                                                double time,
                                                SourceLocation* location) {
  PerformanceMonitor* monitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (!monitor)
    return;
  monitor->reportGenericViolation(violation, text, time, location);
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
}

PerformanceMonitor::~PerformanceMonitor() {
  shutdown();
}

void PerformanceMonitor::subscribe(Violation violation,
                                   double threshold,
                                   Client* client) {
  DCHECK(violation < kAfterLast);
  ClientThresholds* clientThresholds = m_subscriptions.get(violation);
  if (!clientThresholds) {
    clientThresholds = new ClientThresholds();
    m_subscriptions.set(violation, clientThresholds);
  }
  clientThresholds->set(client, threshold);
  updateInstrumentation();
}

void PerformanceMonitor::unsubscribeAll(Client* client) {
  for (const auto& it : m_subscriptions)
    it.value->remove(client);
  updateInstrumentation();
}

void PerformanceMonitor::shutdown() {
  m_subscriptions.clear();
  updateInstrumentation();
  Platform::current()->currentThread()->removeTaskTimeObserver(this);
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

void PerformanceMonitor::alwaysWillExecuteScript(ExecutionContext* context) {
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

void PerformanceMonitor::alwaysDidExecuteScript() {
  --m_scriptDepth;
}

void PerformanceMonitor::alwaysWillCallFunction(ExecutionContext* context) {
  alwaysWillExecuteScript(context);
  if (!m_enabled)
    return;
  if (m_scriptDepth == 1 && m_thresholds[m_handlerType])
    m_scriptStartTime = WTF::monotonicallyIncreasingTime();
}

void PerformanceMonitor::alwaysDidCallFunction(
    v8::Local<v8::Function> function) {
  alwaysDidExecuteScript();
  if (!m_enabled)
    return;
  if (m_scriptDepth)
    return;
  if (m_handlerType == kAfterLast)
    return;
  double threshold = m_thresholds[m_handlerType];
  if (!threshold)
    return;

  double time = WTF::monotonicallyIncreasingTime() - m_scriptStartTime;
  if (time < threshold)
    return;
  String name = m_handlerName ? m_handlerName : m_handlerAtomicName;
  String text = String::format("'%s' handler took %ldms", name.utf8().data(),
                               lround(time * 1000));
  reportGenericViolation(m_handlerType, text, time,
                         SourceLocation::fromFunction(function).get());
}

void PerformanceMonitor::willUpdateLayout() {
  if (m_thresholds[kLongLayout] && m_scriptDepth && !m_layoutDepth)
    m_layoutStartTime = WTF::monotonicallyIncreasingTime();
  ++m_layoutDepth;
}

void PerformanceMonitor::didUpdateLayout() {
  --m_layoutDepth;
  if (m_thresholds[kLongLayout] && m_scriptDepth && !m_layoutDepth) {
    m_perTaskStyleAndLayoutTime +=
        WTF::monotonicallyIncreasingTime() - m_layoutStartTime;
  }
}

void PerformanceMonitor::willRecalculateStyle() {
  if (m_thresholds[kLongLayout] && m_scriptDepth)
    m_styleStartTime = WTF::monotonicallyIncreasingTime();
}

void PerformanceMonitor::didRecalculateStyle() {
  if (m_thresholds[kLongLayout] && m_scriptDepth) {
    m_perTaskStyleAndLayoutTime +=
        WTF::monotonicallyIncreasingTime() - m_styleStartTime;
  }
}

void PerformanceMonitor::willProcessTask(scheduler::TaskQueue*,
                                         double startTime) {
  // Reset m_taskExecutionContext. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  m_taskExecutionContext = nullptr;
  m_taskHasMultipleContexts = false;

  if (!m_enabled)
    return;
  m_scriptDepth = 0;
  m_scriptStartTime = 0;
  m_layoutStartTime = 0;
  m_layoutDepth = 0;
  m_styleStartTime = 0;
  m_perTaskStyleAndLayoutTime = 0;
  m_handlerType = Violation::kAfterLast;
  m_handlerDepth = 0;
}

void PerformanceMonitor::didProcessTask(scheduler::TaskQueue*,
                                        double startTime,
                                        double endTime) {
  if (!m_enabled)
    return;
  double layoutThreshold = m_thresholds[kLongLayout];
  if (layoutThreshold && m_perTaskStyleAndLayoutTime > layoutThreshold) {
    ClientThresholds* clientThresholds = m_subscriptions.get(kLongLayout);
    DCHECK(clientThresholds);
    for (const auto& it : *clientThresholds) {
      if (it.value < m_perTaskStyleAndLayoutTime)
        it.key->reportLongLayout(m_perTaskStyleAndLayoutTime);
    }
  }

  double taskTime = endTime - startTime;
  if (m_thresholds[kLongTask] && taskTime > m_thresholds[kLongTask]) {
    ClientThresholds* clientThresholds = m_subscriptions.get(kLongTask);
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

void PerformanceMonitor::reportGenericViolation(Violation violation,
                                                const String& text,
                                                double time,
                                                SourceLocation* location) {
  ClientThresholds* clientThresholds = m_subscriptions.get(violation);
  if (!clientThresholds)
    return;
  for (const auto& it : *clientThresholds) {
    if (it.value < time)
      it.key->reportGenericViolation(violation, text, time, location);
  }
}

DEFINE_TRACE(PerformanceMonitor) {
  visitor->trace(m_localRoot);
  visitor->trace(m_taskExecutionContext);
  visitor->trace(m_subscriptions);
}

}  // namespace blink
