// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorWebPerfAgent.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/inspector/InspectedFrames.h"
#include "public/platform/Platform.h"

namespace blink {

InspectorWebPerfAgent::InspectorWebPerfAgent(InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames)
{
    Platform::current()->currentThread()->addTaskTimeObserver(this);
    Platform::current()->currentThread()->addTaskObserver(this);
}

InspectorWebPerfAgent::~InspectorWebPerfAgent()
{
    Platform::current()->currentThread()->removeTaskTimeObserver(this);
    Platform::current()->currentThread()->removeTaskObserver(this);
}

void InspectorWebPerfAgent::willExecuteScript(ExecutionContext* context)
{
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

void InspectorWebPerfAgent::didExecuteScript()
{
}

void InspectorWebPerfAgent::willProcessTask()
{
    // Reset m_frameContextLocations. We don't clear this in didProcessTask
    // as it is needed in ReportTaskTime which occurs after didProcessTask.
    m_frameContextLocations.clear();
}

void InspectorWebPerfAgent::didProcessTask()
{
}

void InspectorWebPerfAgent::ReportTaskTime(double startTime, double endTime)
{
}

DEFINE_TRACE(InspectorWebPerfAgent)
{
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_frameContextLocations);
}

} // namespace blink
