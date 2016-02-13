/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InjectedScriptHost_h
#define InjectedScriptHost_h

#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class V8EventListenerInfo;
class JSONValue;
class V8DebuggerImpl;
class V8DebuggerAgentImpl;

// SECURITY NOTE: Although the InjectedScriptHost is intended for use solely by the inspector,
// a reference to the InjectedScriptHost may be leaked to the page being inspected. Thus, the
// InjectedScriptHost must never implemment methods that have more power over the page than the
// page already has itself (e.g. origin restriction bypasses).

class InjectedScriptHost : public RefCounted<InjectedScriptHost> {
public:
    static PassRefPtr<InjectedScriptHost> create(V8DebuggerImpl*);
    ~InjectedScriptHost();

    void setClearConsoleCallback(PassOwnPtr<V8RuntimeAgent::ClearConsoleCallback>);
    void setInspectObjectCallback(PassOwnPtr<V8RuntimeAgent::InspectCallback>);
    void setDebuggerAgent(V8DebuggerAgentImpl* debuggerAgent) { m_debuggerAgent = debuggerAgent; }

    void disconnect();

    void addInspectedObject(PassOwnPtr<V8RuntimeAgent::Inspectable>);
    void clearInspectedObjects();
    V8RuntimeAgent::Inspectable* inspectedObject(unsigned num);

    void inspectImpl(PassRefPtr<JSONValue> objectToInspect, PassRefPtr<JSONValue> hints);

    void clearConsoleMessages();
    void debugFunction(const String& scriptId, int lineNumber, int columnNumber);
    void undebugFunction(const String& scriptId, int lineNumber, int columnNumber);
    void monitorFunction(const String& scriptId, int lineNumber, int columnNumber, const String& functionName);
    void unmonitorFunction(const String& scriptId, int lineNumber, int columnNumber);

    V8DebuggerImpl* debugger() { return m_debugger; }

    // FIXME: store this template in per isolate data
    void setWrapperTemplate(v8::Local<v8::FunctionTemplate> wrapperTemplate, v8::Isolate* isolate) { m_wrapperTemplate.Reset(isolate, wrapperTemplate); }
    v8::Local<v8::FunctionTemplate> wrapperTemplate(v8::Isolate* isolate) { return v8::Local<v8::FunctionTemplate>::New(isolate, m_wrapperTemplate); }

private:
    InjectedScriptHost(V8DebuggerImpl*);

    V8DebuggerImpl* m_debugger;
    V8DebuggerAgentImpl* m_debuggerAgent;
    OwnPtr<V8RuntimeAgent::InspectCallback> m_inspectCallback;
    OwnPtr<V8RuntimeAgent::ClearConsoleCallback> m_clearConsoleCallback;
    Vector<OwnPtr<V8RuntimeAgent::Inspectable>> m_inspectedObjects;
    v8::Global<v8::FunctionTemplate> m_wrapperTemplate;
};

} // namespace blink

#endif // InjectedScriptHost_h
