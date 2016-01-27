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

#include "core/inspector/InjectedScriptManager.h"

#include "bindings/core/v8/ScriptValue.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InjectedScriptNative.h"
#include "core/inspector/RemoteObjectId.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8DebuggerClient.h"
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
    m_idToInjectedScript.remove(contextId);
    return contextId;
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

String InjectedScriptManager::injectedScriptSource()
{
    const WebData& injectedScriptSourceResource = Platform::current()->loadResource("InjectedScriptSource.js");
    return String(injectedScriptSourceResource.data(), injectedScriptSourceResource.size());
}

InjectedScript* InjectedScriptManager::injectedScriptFor(v8::Local<v8::Context> context)
{
    v8::Context::Scope scope(context);
    int contextId = V8Debugger::contextId(context);

    IdToInjectedScriptMap::iterator it = m_idToInjectedScript.find(contextId);
    if (it != m_idToInjectedScript.end())
        return it->value.get();

    if (!m_client->callingContextCanAccessContext(context))
        return nullptr;

    RefPtr<InjectedScriptNative> injectedScriptNative = adoptRef(new InjectedScriptNative(context->GetIsolate()));
    v8::Local<v8::Object> injectedScriptValue = createInjectedScript(injectedScriptSource(), context, contextId, injectedScriptNative.get());
    OwnPtr<InjectedScript> result = adoptPtr(new InjectedScript(injectedScriptValue, m_client, injectedScriptNative.release(), contextId));
    InjectedScript* resultPtr = result.get();
    if (m_customObjectFormatterEnabled)
        result->setCustomObjectFormatterEnabled(m_customObjectFormatterEnabled);
    m_idToInjectedScript.set(contextId, result.release());
    return resultPtr;
}

} // namespace blink
