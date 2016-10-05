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
static const char* unknownAttribution = "unknown";
static const char* ambiguousAttribution = "multiple-contexts";

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
  performance->addLongTaskTiming(
      startTime, endTime,
      sanitizedLongTaskName(m_frameContextLocations, m_localFrame));
}

String InspectorWebPerfAgent::sanitizedLongTaskName(
    const HeapHashSet<Member<Location>>& frameContextLocations,
    Frame* rootFrame) {
  if (frameContextLocations.size() == 0) {
    // Unable to attribute as no script was involved.
    return unknownAttribution;
  }
  if (frameContextLocations.size() > 1) {
    // Unable to attribute, multiple script execution contents were involved.
    return ambiguousAttribution;
  }
  // Exactly one culprit location, attribute based on origin boundary.
  DCHECK_EQ(1u, frameContextLocations.size());
  Location* culpritLocation = *frameContextLocations.begin();
  if (canAccessOrigin(rootFrame, culpritLocation->frame())) {
    // For same origin, it's safe to to return culprit location URL.
    return culpritLocation->href();
  }
  return "cross-origin";
}

DEFINE_TRACE(InspectorWebPerfAgent) {
  visitor->trace(m_localFrame);
  visitor->trace(m_frameContextLocations);
}

}  // namespace blink
