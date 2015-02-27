/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorInspectorAgent_h
#define InspectorInspectorAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class InjectedScriptManager;
class InspectorController;
class JSONObject;

typedef String ErrorString;

class InspectorInspectorAgent final : public InspectorBaseAgent<InspectorInspectorAgent>, public InspectorBackendDispatcher::InspectorCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorInspectorAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorInspectorAgent> create(InspectorController* inspectorController, InjectedScriptManager* injectedScriptManager)
    {
        return adoptPtrWillBeNoop(new InspectorInspectorAgent(inspectorController, injectedScriptManager));
    }

    virtual ~InspectorInspectorAgent();
    DECLARE_VIRTUAL_TRACE();

    // Inspector front-end API.
    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    virtual void reset(ErrorString*) override;

    virtual void init() override;
    virtual void setFrontend(InspectorFrontend*) override;
    virtual void clearFrontend() override;

    void domContentLoadedEventFired(LocalFrame*);

    bool hasFrontend() const { return m_frontend; }

    // Generic code called from custom implementations.
    void evaluateForTestInFrontend(long testCallId, const String& script);

    void inspect(PassRefPtr<TypeBuilder::Runtime::RemoteObject> objectToInspect, PassRefPtr<JSONObject> hints);

private:
    InspectorInspectorAgent(InspectorController*, InjectedScriptManager*);

    RawPtrWillBeMember<InspectorController> m_inspectorController;
    InspectorFrontend::Inspector* m_frontend;
    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;

    Vector<pair<long, String> > m_pendingEvaluateTestCommands;
};

} // namespace blink

#endif // !defined(InspectorInspectorAgent_h)
