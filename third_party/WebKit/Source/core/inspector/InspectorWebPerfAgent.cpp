// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorWebPerfAgent.h"

#include "core/InstrumentingAgents.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectedFrames.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {
static const double kLongTaskThresholdMillis = 50.0;
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
  // TODO(panicker): Confirm this pending security review.
  return securityOrigin1->canAccess(securityOrigin2);
}
}  // namespace

InspectorWebPerfAgent::InspectorWebPerfAgent(LocalFrame* localFrame)
    : m_localFrame(localFrame) {}

InspectorWebPerfAgent::~InspectorWebPerfAgent() {
  DCHECK(!m_enabled);
}

void InspectorWebPerfAgent::enable() {
  // Update usage count.
  UseCounter::count(m_localFrame, UseCounter::LongTaskObserver);
  Platform::current()->currentThread()->addTaskTimeObserver(this);
  Platform::current()->currentThread()->addTaskObserver(this);
  m_localFrame->instrumentingAgents()->addInspectorWebPerfAgent(this);
  m_enabled = true;
}

void InspectorWebPerfAgent::disable() {
  Platform::current()->currentThread()->removeTaskTimeObserver(this);
  Platform::current()->currentThread()->removeTaskObserver(this);
  m_localFrame->instrumentingAgents()->removeInspectorWebPerfAgent(this);
  m_enabled = false;
}

void InspectorWebPerfAgent::willExecuteScript(ExecutionContext* context) {
  // Heuristic for minimal frame context attribution: note the Location URL
  // for each script execution. When a long task is encountered,
  // if there is only one Location URL involved, then report it.
  // Otherwise don't report Location URL.
  // NOTE: This heuristic is imperfect and will be improved in V2 API.
  // In V2, timing of script execution along with style & layout updates will be
  // accounted for detailed and more accurate attribution.
  if (context->isDocument())
    m_frameContextLocations.add(toDocument(context)->location());
}

void InspectorWebPerfAgent::didExecuteScript() {}

void InspectorWebPerfAgent::willProcessTask() {
  // Reset m_frameContextLocations. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  m_frameContextLocations.clear();
}

void InspectorWebPerfAgent::didProcessTask() {}

void InspectorWebPerfAgent::ReportTaskTime(scheduler::TaskQueue*,
                                           double startTime,
                                           double endTime) {
  if (((endTime - startTime) * 1000) <= kLongTaskThresholdMillis)
    return;
  DOMWindow* domWindow = m_localFrame->domWindow();
  if (!domWindow)
    return;
  Performance* performance = DOMWindowPerformance::performance(*domWindow);
  DCHECK(performance);

  std::pair<String, DOMWindow*> attribution =
      sanitizedAttribution(m_frameContextLocations, m_localFrame);
  performance->addLongTaskTiming(startTime, endTime, attribution.first,
                                 attribution.second);
}

/**
 * Report sanitized name based on cross-origin policy.
 * See detailed Security doc here: http://bit.ly/2duD3F7
 */
std::pair<String, DOMWindow*> InspectorWebPerfAgent::sanitizedAttribution(
    const HeapHashSet<Member<Location>>& frameContextLocations,
    Frame* observerFrame) {
  if (frameContextLocations.size() == 0) {
    // Unable to attribute as no script was involved.
    return std::make_pair(kUnknownAttribution, nullptr);
  }
  if (frameContextLocations.size() > 1) {
    // Unable to attribute, multiple script execution contents were involved.
    return std::make_pair(kAmbugiousAttribution, nullptr);
  }
  // Exactly one culprit location, attribute based on origin boundary.
  DCHECK_EQ(1u, frameContextLocations.size());
  Location* culpritLocation = *frameContextLocations.begin();
  if (canAccessOrigin(observerFrame, culpritLocation->frame())) {
    // From accessible frames or same origin, return culprit location URL.
    return std::make_pair(kSameOriginAttribution,
                          culpritLocation->frame()->domWindow());
  }
  // For cross-origin, if the culprit is the descendant or ancestor of
  // observer then indicate the *closest* cross-origin frame between
  // the observer and the culprit, in the corresponding direction.
  if (culpritLocation->frame()->tree().isDescendantOf(observerFrame)) {
    // If the culprit is a descendant of the observer, then walk up the tree
    // from culprit to observer, and report the *last* cross-origin (from
    // observer) frame.  If no intermediate cross-origin frame is found, then
    // report the culprit directly.
    Frame* lastCrossOriginFrame = culpritLocation->frame();
    for (Frame* frame = culpritLocation->frame(); frame != observerFrame;
         frame = frame->tree().parent()) {
      if (!canAccessOrigin(observerFrame, frame)) {
        lastCrossOriginFrame = frame;
      }
    }
    return std::make_pair(kDescendantAttribution,
                          lastCrossOriginFrame->domWindow());
  }
  if (observerFrame->tree().isDescendantOf(culpritLocation->frame())) {
    return std::make_pair(kAncestorAttribution, nullptr);
  }
  return std::make_pair(kCrossOriginAttribution, nullptr);
}

DEFINE_TRACE(InspectorWebPerfAgent) {
  visitor->trace(m_localFrame);
  visitor->trace(m_frameContextLocations);
}

}  // namespace blink
