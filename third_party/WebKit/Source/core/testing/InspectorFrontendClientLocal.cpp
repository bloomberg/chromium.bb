/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "InspectorFrontendClientLocal.h"

#include "bindings/v8/ScriptObject.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendHost.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/Timer.h"
#include <wtf/Deque.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class InspectorBackendDispatchTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorBackendDispatchTask(InspectorController* inspectorController)
        : m_inspectorController(inspectorController)
        , m_timer(this, &InspectorBackendDispatchTask::onTimer)
    {
    }

    void dispatch(const String& message)
    {
        m_messages.append(message);
        if (!m_timer.isActive())
            m_timer.startOneShot(0);
    }

    void reset()
    {
        m_messages.clear();
        m_timer.stop();
    }

    void onTimer(Timer<InspectorBackendDispatchTask>*)
    {
        if (!m_messages.isEmpty()) {
            // Dispatch can lead to the timer destruction -> schedule the next shot first.
            m_timer.startOneShot(0);
            m_inspectorController->dispatchMessageFromFrontend(m_messages.takeFirst());
        }
    }

private:
    InspectorController* m_inspectorController;
    Timer<InspectorBackendDispatchTask> m_timer;
    Deque<String> m_messages;
};

InspectorFrontendClientLocal::InspectorFrontendClientLocal(InspectorController* inspectorController, Page* frontendPage)
    : m_inspectorController(inspectorController)
    , m_frontendPage(frontendPage)
{
    m_frontendPage->settings()->setAllowFileAccessFromFileURLs(true);
    m_dispatchTask = adoptPtr(new InspectorBackendDispatchTask(inspectorController));
}

InspectorFrontendClientLocal::~InspectorFrontendClientLocal()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    m_frontendPage = 0;
    m_inspectorController = 0;
}

void InspectorFrontendClientLocal::windowObjectCleared()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();
    ScriptState* frontendScriptState = scriptStateFromPage(debuggerWorld(), m_frontendPage);
    m_frontendHost = InspectorFrontendHost::create(this, m_frontendPage);
    ScriptGlobalObject::set(frontendScriptState, "InspectorFrontendHost", m_frontendHost.get());
}

void InspectorFrontendClientLocal::sendMessageToBackend(const String& message)
{
    m_dispatchTask->dispatch(message);
}

} // namespace WebCore

