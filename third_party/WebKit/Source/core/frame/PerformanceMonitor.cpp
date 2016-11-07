// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/InstrumentingAgents.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {
static const double kLongTaskThresholdMillis = 50.0;
static const double kLongTaskWarningThresholdMillis = 150.0;

static const double kSyncLayoutThresholdMillis = 30.0;
static const double kSyncLayoutWarningThresholdMillis = 60.0;

static const char kUnknownAttribution[] = "unknown";
static const char kAmbugiousAttribution[] = "multiple-contexts";
static const char kSameOriginAttribution[] = "same-origin";
static const char kAncestorAttribution[] = "cross-origin-ancestor";
static const char kDescendantAttribution[] = "cross-origin-descendant";
static const char kCrossOriginAttribution[] = "cross-origin-unreachable";

bool canAccessOrigin(Frame* frame1, Frame* frame2) {
  SecurityOrigin* securityOrigin1 =
      frame1->securityContext()->getSecurityOrigin();
  SecurityOrigin* securityOrigin2 =
      frame2->securityContext()->getSecurityOrigin();
  return securityOrigin1->canAccess(securityOrigin2);
}

}  // namespace

// static
void PerformanceMonitor::performanceObserverAdded(Performance* performance) {
  LocalFrame* frame = performance->frame();
  PerformanceMonitor* monitor = frame->performanceMonitor();
  monitor->m_webPerformanceObservers.add(performance);
  monitor->updateInstrumentation();
}

// static
void PerformanceMonitor::performanceObserverRemoved(Performance* performance) {
  LocalFrame* frame = performance->frame();
  if (!frame) {
    // TODO: instrument local frame removal in order to clean up.
    return;
  }
  PerformanceMonitor* monitor = frame->performanceMonitor();
  monitor->m_webPerformanceObservers.remove(performance);
  monitor->updateInstrumentation();
}

// static
void PerformanceMonitor::willExecuteScript(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (performanceMonitor)
    performanceMonitor->innerWillExecuteScript(context);
}

// static
void PerformanceMonitor::didExecuteScript(ExecutionContext* context) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (performanceMonitor)
    performanceMonitor->didExecuteScript();
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
bool PerformanceMonitor::enabled(ExecutionContext* context) {
  return PerformanceMonitor::instrumentingMonitor(context);
}

// static
void PerformanceMonitor::logViolation(MessageLevel level,
                                      ExecutionContext* context,
                                      const String& messageText) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (performanceMonitor)
    performanceMonitor->logViolation(level, messageText);
}

// static
void PerformanceMonitor::logViolation(
    MessageLevel level,
    ExecutionContext* context,
    const String& messageText,
    std::unique_ptr<SourceLocation> location) {
  PerformanceMonitor* performanceMonitor =
      PerformanceMonitor::instrumentingMonitor(context);
  if (performanceMonitor)
    performanceMonitor->logViolation(level, messageText, std::move(location));
}

// static
PerformanceMonitor* PerformanceMonitor::instrumentingMonitor(
    ExecutionContext* context) {
  if (!context->isDocument())
    return nullptr;
  LocalFrame* frame = toDocument(context)->frame();
  if (!frame)
    return nullptr;
  PerformanceMonitor* monitor = frame->performanceMonitor();
  return monitor->m_enabled ? monitor : nullptr;
}

PerformanceMonitor::PerformanceMonitor(LocalFrame* localRoot)
    : m_localRoot(localRoot) {}

PerformanceMonitor::~PerformanceMonitor() {}

void PerformanceMonitor::setLoggingEnabled(bool enabled) {
  m_loggingEnabled = enabled;
  updateInstrumentation();
}

void PerformanceMonitor::updateInstrumentation() {
  bool shouldEnable = m_loggingEnabled || m_webPerformanceObservers.size();
  if (shouldEnable == m_enabled)
    return;
  if (shouldEnable) {
    UseCounter::count(m_localRoot, UseCounter::LongTaskObserver);
    Platform::current()->currentThread()->addTaskTimeObserver(this);
    Platform::current()->currentThread()->addTaskObserver(this);
  } else {
    Platform::current()->currentThread()->removeTaskTimeObserver(this);
    Platform::current()->currentThread()->removeTaskObserver(this);
  }
  m_enabled = shouldEnable;
}

void PerformanceMonitor::innerWillExecuteScript(ExecutionContext* context) {
  m_isExecutingScript = true;
  // Heuristic for minimal frame context attribution: note the frame context
  // for each script execution. When a long task is encountered,
  // if there is only one frame context involved, then report it.
  // Otherwise don't report frame context.
  // NOTE: This heuristic is imperfect and will be improved in V2 API.
  // In V2, timing of script execution along with style & layout updates will be
  // accounted for detailed and more accurate attribution.
  if (context->isDocument() && toDocument(context)->frame())
    m_frameContexts.add(toDocument(context)->frame());
}

void PerformanceMonitor::didExecuteScript() {
  m_isExecutingScript = false;
}

void PerformanceMonitor::willUpdateLayout() {
  if (m_isExecutingScript)
    m_layoutStartTime = WTF::monotonicallyIncreasingTime();
}

void PerformanceMonitor::didUpdateLayout() {
  if (m_isExecutingScript) {
    m_perTaskStyleAndLayoutTime +=
        WTF::monotonicallyIncreasingTime() - m_layoutStartTime;
  }
}

void PerformanceMonitor::willRecalculateStyle() {
  if (m_isExecutingScript)
    m_styleStartTime = WTF::monotonicallyIncreasingTime();
}

void PerformanceMonitor::didRecalculateStyle() {
  if (m_isExecutingScript) {
    m_perTaskStyleAndLayoutTime +=
        WTF::monotonicallyIncreasingTime() - m_styleStartTime;
  }
}

void PerformanceMonitor::willProcessTask() {
  m_perTaskStyleAndLayoutTime = 0;
  // Reset m_frameContexts. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  m_frameContexts.clear();
}

void PerformanceMonitor::didProcessTask() {
  if (m_perTaskStyleAndLayoutTime * 1000 < kSyncLayoutThresholdMillis)
    return;

  if (m_loggingEnabled) {
    logViolation(
        m_perTaskStyleAndLayoutTime * 1000 < kSyncLayoutWarningThresholdMillis
            ? InfoMessageLevel
            : WarningMessageLevel,
        String::format("Forced reflow while executing JavaScript took %ldms.",
                       lround(m_perTaskStyleAndLayoutTime * 1000)));
  }
}

void PerformanceMonitor::ReportTaskTime(scheduler::TaskQueue*,
                                        double startTime,
                                        double endTime) {
  double taskTimeMs = (endTime - startTime) * 1000;
  if (taskTimeMs < kLongTaskThresholdMillis)
    return;

  for (Performance* performance : m_webPerformanceObservers) {
    if (!performance->frame())
      continue;
    std::pair<String, DOMWindow*> attribution =
        sanitizedAttribution(m_frameContexts, performance->frame());
    performance->addLongTaskTiming(startTime, endTime, attribution.first,
                                   attribution.second);
  }
  if (m_loggingEnabled) {
    logViolation(taskTimeMs < kLongTaskWarningThresholdMillis
                     ? InfoMessageLevel
                     : WarningMessageLevel,
                 String::format("Long running JavaScript task took %ldms.",
                                lround(taskTimeMs)));
  }
}

void PerformanceMonitor::logViolation(MessageLevel level,
                                      const String& messageText) {
  ConsoleMessage* message =
      ConsoleMessage::create(ViolationMessageSource, level, messageText);
  m_localRoot->console().addMessage(message);
}

void PerformanceMonitor::logViolation(
    MessageLevel level,
    const String& messageText,
    std::unique_ptr<SourceLocation> location) {
  ConsoleMessage* message = ConsoleMessage::create(
      ViolationMessageSource, level, messageText, std::move(location));
  m_localRoot->console().addMessage(message);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
std::pair<String, DOMWindow*> PerformanceMonitor::sanitizedAttribution(
    const HeapHashSet<Member<Frame>>& frames,
    Frame* observerFrame) {
  if (frames.size() == 0) {
    // Unable to attribute as no script was involved.
    return std::make_pair(kUnknownAttribution, nullptr);
  }
  if (frames.size() > 1) {
    // Unable to attribute, multiple script execution contents were involved.
    return std::make_pair(kAmbugiousAttribution, nullptr);
  }
  // Exactly one culprit location, attribute based on origin boundary.
  DCHECK_EQ(1u, frames.size());
  Frame* culpritFrame = *frames.begin();
  DCHECK(culpritFrame);
  if (canAccessOrigin(observerFrame, culpritFrame)) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(kSameOriginAttribution, culpritFrame->domWindow());
  }
  // For cross-origin, if the culprit is the descendant or ancestor of
  // observer then indicate the *closest* cross-origin frame between
  // the observer and the culprit, in the corresponding direction.
  if (culpritFrame->tree().isDescendantOf(observerFrame)) {
    // If the culprit is a descendant of the observer, then walk up the tree
    // from culprit to observer, and report the *last* cross-origin (from
    // observer) frame.  If no intermediate cross-origin frame is found, then
    // report the culprit directly.
    Frame* lastCrossOriginFrame = culpritFrame;
    for (Frame* frame = culpritFrame; frame != observerFrame;
         frame = frame->tree().parent()) {
      if (!canAccessOrigin(observerFrame, frame)) {
        lastCrossOriginFrame = frame;
      }
    }
    return std::make_pair(kDescendantAttribution,
                          lastCrossOriginFrame->domWindow());
  }
  if (observerFrame->tree().isDescendantOf(culpritFrame)) {
    return std::make_pair(kAncestorAttribution, nullptr);
  }
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

DEFINE_TRACE(PerformanceMonitor) {
  visitor->trace(m_localRoot);
  visitor->trace(m_frameContexts);
  visitor->trace(m_webPerformanceObservers);
}

}  // namespace blink
