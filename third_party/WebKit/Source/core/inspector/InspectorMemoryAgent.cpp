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

#include "config.h"
#include "core/inspector/InspectorMemoryAgent.h"

#include "InspectorFrontend.h"
#include "bindings/v8/ScriptGCEvent.h"
#include "bindings/v8/ScriptProfiler.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/inspector/BindingVisitors.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorDOMStorageAgent.h"
#include "core/loader/cache/MemoryCache.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/JSONValues.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/NonCopyingSort.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringImpl.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

InspectorMemoryAgent::~InspectorMemoryAgent()
{
}

void InspectorMemoryAgent::getDOMCounters(ErrorString*, int* documents, int* nodes, int* jsEventListeners)
{
    *documents = InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
    *nodes = InspectorCounters::counterValue(InspectorCounters::NodeCounter);
    *jsEventListeners = ThreadLocalInspectorCounters::current().counterValue(ThreadLocalInspectorCounters::JSEventListenerCounter);
}

InspectorMemoryAgent::InspectorMemoryAgent(InstrumentingAgents* instrumentingAgents, InspectorClient* client, InspectorCompositeState* state, Page* page)
    : InspectorBaseAgent<InspectorMemoryAgent>("Memory", instrumentingAgents, state)
    , m_inspectorClient(client)
    , m_page(page)
    , m_frontend(0)
{
}

void InspectorMemoryAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_frontend = frontend->memory();
}

void InspectorMemoryAgent::clearFrontend()
{
    m_frontend = 0;
}

} // namespace WebCore

