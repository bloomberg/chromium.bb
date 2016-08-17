// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorWebPerfAgent.h"

#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"

namespace blink {

InspectorWebPerfAgent::InspectorWebPerfAgent(InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames)
{
}

InspectorWebPerfAgent::~InspectorWebPerfAgent()
{
}

void InspectorWebPerfAgent::willExecuteScript(ExecutionContext* context)
{
}

void InspectorWebPerfAgent::didExecuteScript()
{
}

DEFINE_TRACE(InspectorWebPerfAgent)
{
    visitor->trace(m_inspectedFrames);
}

} // namespace blink
