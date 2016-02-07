/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/inspector/v8/InjectedScriptManager.h"

#include "core/inspector/v8/InjectedScript.h"
#include "core/inspector/v8/InjectedScriptHost.h"
#include "core/inspector/v8/InjectedScriptNative.h"
#include "core/inspector/v8/RemoteObjectId.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8DebuggerClient.h"
#include "core/inspector/v8/V8InjectedScriptHost.h"
#include "core/inspector/v8/V8StringUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

PassOwnPtr<InjectedScriptManager> InjectedScriptManager::create(V8DebuggerClient* client)
{
    return adoptPtr(new InjectedScriptManager(client));
}

InjectedScriptManager::InjectedScriptManager(V8DebuggerClient* client)
    : m_injectedScriptHost(InjectedScriptHost::create())
    , m_customObjectFormatterEnabled(false)
    , m_client(client)
{
}

InjectedScriptManager::~InjectedScriptManager()
{
}

void InjectedScriptManager::disconnect()
{
    m_injectedScriptHost->disconnect();
    m_injectedScriptHost.clear();
}

InjectedScriptHost* InjectedScriptManager::injectedScriptHost()
{
    return m_injectedScriptHost.get();
}

InjectedScript* InjectedScriptManager::findInjectedScript(int id) const
{
    IdToInjectedScriptMap::const_iterator it = m_idToInjectedScript.find(id);
    if (it != m_idToInjectedScript.end())
        return it->value.get();
    return nullptr;
}

InjectedScript* InjectedScriptManager::findInjectedScript(RemoteObjectIdBase* objectId) const
{
    return objectId ? findInjectedScript(objectId->contextId()) : nullptr;
}

void InjectedScriptManager::discardInjectedScripts()
{
    m_idToInjectedScript.clear();
}

int InjectedScriptManager::discardInjectedScriptFor(v8::Local<v8::Context> context)
{
    int contextId = V8Debugger::contextId(context);
    discardInjectedScript(contextId);
    return contextId;
}

void InjectedScriptManager::discardInjectedScript(int contextId)
{
    m_idToInjectedScript.remove(contextId);
}

void InjectedScriptManager::releaseObjectGroup(const String& objectGroup)
{
    Vector<int> keys;
    keys.appendRange(m_idToInjectedScript.keys().begin(), m_idToInjectedScript.keys().end());
    for (auto& key : keys) {
        IdToInjectedScriptMap::iterator s = m_idToInjectedScript.find(key);
        if (s != m_idToInjectedScript.end())
            s->value->releaseObjectGroup(objectGroup); // m_idToInjectedScript may change here.
    }
}

void InjectedScriptManager::setCustomObjectFormatterEnabled(bool enabled)
{
    m_customObjectFormatterEnabled = enabled;
    IdToInjectedScriptMap::iterator end = m_idToInjectedScript.end();
    for (IdToInjectedScriptMap::iterator it = m_idToInjectedScript.begin(); it != end; ++it) {
        it->value->setCustomObjectFormatterEnabled(enabled);
    }
}

InjectedScript* InjectedScriptManager::injectedScriptFor(v8::Local<v8::Context> context)
{
    v8::Context::Scope scope(context);
    int contextId = V8Debugger::contextId(context);

    IdToInjectedScriptMap::iterator it = m_idToInjectedScript.find(contextId);
    if (it != m_idToInjectedScript.end())
        return it->value.get();

    v8::Local<v8::Context> callingContext = context->GetIsolate()->GetCallingContext();
    if (!callingContext.IsEmpty() && !m_client->callingContextCanAccessContext(callingContext, context))
        return nullptr;

    RefPtr<InjectedScriptNative> injectedScriptNative = adoptRef(new InjectedScriptNative(context->GetIsolate()));

    const WebData& injectedScriptSourceResource = Platform::current()->loadResource("InjectedScriptSource.js");
    String injectedScriptSource(injectedScriptSourceResource.data(), injectedScriptSourceResource.size());

    v8::Local<v8::Object> object = createInjectedScript(injectedScriptSource, context, contextId, injectedScriptNative.get());
    OwnPtr<InjectedScript> result = adoptPtr(new InjectedScript(this, context, object, m_client, injectedScriptNative.release(), contextId));
    InjectedScript* resultPtr = result.get();
    if (m_customObjectFormatterEnabled)
        result->setCustomObjectFormatterEnabled(m_customObjectFormatterEnabled);
    m_idToInjectedScript.set(contextId, result.release());
    return resultPtr;
}

v8::Local<v8::Object> InjectedScriptManager::createInjectedScript(const String& source, v8::Local<v8::Context> context, int id, InjectedScriptNative* injectedScriptNative)
{
    v8::Isolate* isolate = context->GetIsolate();
    v8::Context::Scope scope(context);

    v8::Local<v8::FunctionTemplate> wrapperTemplate = m_injectedScriptHost->wrapperTemplate(isolate);
    if (wrapperTemplate.IsEmpty()) {
        wrapperTemplate = V8InjectedScriptHost::createWrapperTemplate(isolate);
        m_injectedScriptHost->setWrapperTemplate(wrapperTemplate, isolate);
    }

    v8::Local<v8::Object> scriptHostWrapper = V8InjectedScriptHost::wrap(m_client, wrapperTemplate, context, m_injectedScriptHost);
    if (scriptHostWrapper.IsEmpty())
        return v8::Local<v8::Object>();

    injectedScriptNative->setOnInjectedScriptHost(scriptHostWrapper);

    // Inject javascript into the context. The compiled script is supposed to evaluate into
    // a single anonymous function(it's anonymous to avoid cluttering the global object with
    // inspector's stuff) the function is called a few lines below with InjectedScriptHost wrapper,
    // injected script id and explicit reference to the inspected global object. The function is expected
    // to create and configure InjectedScript instance that is going to be used by the inspector.
    v8::Local<v8::Value> value;
    if (!m_client->compileAndRunInternalScript(toV8String(isolate, source)).ToLocal(&value))
        return v8::Local<v8::Object>();
    ASSERT(value->IsFunction());

    v8::Local<v8::Object> windowGlobal = context->Global();
    v8::Local<v8::Value> info[] = { scriptHostWrapper, windowGlobal, v8::Number::New(context->GetIsolate(), id) };
    v8::Local<v8::Value> injectedScriptValue;
    if (!m_client->callInternalFunction(v8::Local<v8::Function>::Cast(value), windowGlobal, WTF_ARRAY_LENGTH(info), info).ToLocal(&injectedScriptValue))
        return v8::Local<v8::Object>();
    if (!injectedScriptValue->IsObject())
        return v8::Local<v8::Object>();
    return injectedScriptValue.As<v8::Object>();
}

} // namespace blink
