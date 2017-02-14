/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/timing/Performance.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/origin_trials/OriginTrials.h"
#include "core/timing/PerformanceTiming.h"

static const double kLongTaskThreshold = 0.05;

static const char kUnknownAttribution[] = "unknown";
static const char kAmbiguousAttribution[] = "multiple-contexts";
static const char kSameOriginAttribution[] = "same-origin";
static const char kSameOriginSelfAttribution[] = "self";
static const char kSameOriginAncestorAttribution[] = "same-origin-ancestor";
static const char kSameOriginDescendantAttribution[] = "same-origin-descendant";
static const char kCrossOriginAncestorAttribution[] = "cross-origin-ancestor";
static const char kCrossOriginDescendantAttribution[] =
    "cross-origin-descendant";
static const char kCrossOriginAttribution[] = "cross-origin-unreachable";

namespace blink {

namespace {

String getFrameAttribute(HTMLFrameOwnerElement* frameOwner,
                         const QualifiedName& attrName,
                         bool truncate) {
  String attrValue;
  if (frameOwner->hasAttribute(attrName)) {
    attrValue = frameOwner->getAttribute(attrName);
    if (truncate && attrValue.length() > 100)
      attrValue = attrValue.substring(0, 100);  // Truncate to 100 chars
  }
  return attrValue;
}

const char* sameOriginAttribution(Frame* observerFrame, Frame* culpritFrame) {
  if (observerFrame == culpritFrame)
    return kSameOriginSelfAttribution;
  if (observerFrame->tree().isDescendantOf(culpritFrame))
    return kSameOriginAncestorAttribution;
  if (culpritFrame->tree().isDescendantOf(observerFrame))
    return kSameOriginDescendantAttribution;
  return kSameOriginAttribution;
}

}  // namespace

static double toTimeOrigin(LocalFrame* frame) {
  if (!frame)
    return 0.0;

  Document* document = frame->document();
  if (!document)
    return 0.0;

  DocumentLoader* loader = document->loader();
  if (!loader)
    return 0.0;

  return loader->timing().referenceMonotonicTime();
}

Performance::Performance(LocalFrame* frame)
    : PerformanceBase(
          toTimeOrigin(frame),
          TaskRunnerHelper::get(TaskType::PerformanceTimeline, frame)),
      ContextLifecycleObserver(frame ? frame->document() : nullptr) {}

Performance::~Performance() {
}

void Performance::contextDestroyed(ExecutionContext* destroyedContext) {
  toDocument(destroyedContext)
      ->frame()
      ->performanceMonitor()
      ->unsubscribeAll(this);
}

ExecutionContext* Performance::getExecutionContext() const {
  if (!frame())
    return nullptr;
  return frame()->document();
}

MemoryInfo* Performance::memory() {
  return MemoryInfo::create();
}

PerformanceNavigation* Performance::navigation() const {
  if (!m_navigation)
    m_navigation = PerformanceNavigation::create(frame());

  return m_navigation.get();
}

PerformanceTiming* Performance::timing() const {
  if (!m_timing)
    m_timing = PerformanceTiming::create(frame());

  return m_timing.get();
}

void Performance::updateLongTaskInstrumentation() {
  DCHECK(frame());
  if (!frame()->document())
    return;

  if (hasObserverFor(PerformanceEntry::LongTask)) {
    UseCounter::count(frame()->localFrameRoot(), UseCounter::LongTaskObserver);
    frame()->performanceMonitor()->subscribe(PerformanceMonitor::kLongTask,
                                             kLongTaskThreshold, this);
  } else {
    frame()->performanceMonitor()->unsubscribeAll(this);
  }
}

ScriptValue Performance::toJSONForBinding(ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.add("timing", timing()->toJSONForBinding(scriptState));
  result.add("navigation", navigation()->toJSONForBinding(scriptState));
  return result.scriptValue();
}

DEFINE_TRACE(Performance) {
  visitor->trace(m_navigation);
  visitor->trace(m_timing);
  ContextLifecycleObserver::trace(visitor);
  PerformanceBase::trace(visitor);
  PerformanceMonitor::Client::trace(visitor);
}

static bool canAccessOrigin(Frame* frame1, Frame* frame2) {
  SecurityOrigin* securityOrigin1 =
      frame1->securityContext()->getSecurityOrigin();
  SecurityOrigin* securityOrigin2 =
      frame2->securityContext()->getSecurityOrigin();
  return securityOrigin1->canAccess(securityOrigin2);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
// static
std::pair<String, DOMWindow*> Performance::sanitizedAttribution(
    ExecutionContext* taskContext,
    bool hasMultipleContexts,
    LocalFrame* observerFrame) {
  if (hasMultipleContexts) {
    // Unable to attribute, multiple script execution contents were involved.
    return std::make_pair(kAmbiguousAttribution, nullptr);
  }

  if (!taskContext || !taskContext->isDocument() ||
      !toDocument(taskContext)->frame()) {
    // Unable to attribute as no script was involved.
    return std::make_pair(kUnknownAttribution, nullptr);
  }

  // Exactly one culprit location, attribute based on origin boundary.
  Frame* culpritFrame = toDocument(taskContext)->frame();
  DCHECK(culpritFrame);
  if (canAccessOrigin(observerFrame, culpritFrame)) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(sameOriginAttribution(observerFrame, culpritFrame),
                          culpritFrame->domWindow());
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
    return std::make_pair(kCrossOriginDescendantAttribution,
                          lastCrossOriginFrame->domWindow());
  }
  if (observerFrame->tree().isDescendantOf(culpritFrame)) {
    return std::make_pair(kCrossOriginAncestorAttribution, nullptr);
  }
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

void Performance::reportLongTask(double startTime,
                                 double endTime,
                                 ExecutionContext* taskContext,
                                 bool hasMultipleContexts) {
  std::pair<String, DOMWindow*> attribution = Performance::sanitizedAttribution(
      taskContext, hasMultipleContexts, frame());
  DOMWindow* culpritDomWindow = attribution.second;
  if (!culpritDomWindow || !culpritDomWindow->frame() ||
      !culpritDomWindow->frame()->deprecatedLocalOwner()) {
    addLongTaskTiming(startTime, endTime, attribution.first, emptyString,
                      emptyString, emptyString);
  } else {
    HTMLFrameOwnerElement* frameOwner =
        culpritDomWindow->frame()->deprecatedLocalOwner();
    addLongTaskTiming(startTime, endTime, attribution.first,
                      getFrameAttribute(frameOwner, HTMLNames::srcAttr, false),
                      getFrameAttribute(frameOwner, HTMLNames::idAttr, false),
                      getFrameAttribute(frameOwner, HTMLNames::nameAttr, true));
  }
}

}  // namespace blink
