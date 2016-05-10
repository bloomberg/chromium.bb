/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/PageRuntimeAgent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/page/Page.h"
#include "platform/weborigin/SecurityOrigin.h"

using blink::protocol::Runtime::ExceptionDetails;

namespace blink {

PageRuntimeAgent::PageRuntimeAgent(V8RuntimeAgent* agent, InspectedFrames* inspectedFrames)
    : InspectorRuntimeAgent(agent)
    , m_inspectedFrames(inspectedFrames)
{
}

PageRuntimeAgent::~PageRuntimeAgent()
{
}

DEFINE_TRACE(PageRuntimeAgent)
{
    visitor->trace(m_inspectedFrames);
    InspectorRuntimeAgent::trace(visitor);
}

void PageRuntimeAgent::enable(ErrorString* errorString)
{
    if (m_enabled)
        return;
    InspectorRuntimeAgent::enable(errorString);
}

void PageRuntimeAgent::disable(ErrorString* errorString)
{
    if (!m_enabled)
        return;
    InspectorRuntimeAgent::disable(errorString);
}

} // namespace blink
