/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef InjectedScriptManager_h
#define InjectedScriptManager_h

#include "core/CoreExport.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class InjectedScript;
class InjectedScriptHost;
class InjectedScriptNative;
class RemoteObjectIdBase;
class V8DebuggerClient;

class CORE_EXPORT InjectedScriptManager {
    WTF_MAKE_NONCOPYABLE(InjectedScriptManager);
    USING_FAST_MALLOC(InjectedScriptManager);
public:
    static PassOwnPtr<InjectedScriptManager> create(V8DebuggerClient*);
    ~InjectedScriptManager();

    void disconnect();

    InjectedScriptHost* injectedScriptHost();

    InjectedScript* injectedScriptFor(v8::Local<v8::Context>);
    InjectedScript* findInjectedScript(int) const;
    InjectedScript* findInjectedScript(RemoteObjectIdBase*) const;
    void discardInjectedScripts();
    int discardInjectedScriptFor(v8::Local<v8::Context>);
    void releaseObjectGroup(const String& objectGroup);
    void setCustomObjectFormatterEnabled(bool);

private:
    explicit InjectedScriptManager(V8DebuggerClient*);

    String injectedScriptSource();
    v8::Local<v8::Object> createInjectedScript(const String& source, v8::Local<v8::Context>, int id, InjectedScriptNative*);

    typedef HashMap<int, OwnPtr<InjectedScript>> IdToInjectedScriptMap;
    IdToInjectedScriptMap m_idToInjectedScript;
    RefPtr<InjectedScriptHost> m_injectedScriptHost;
    bool m_customObjectFormatterEnabled;
    V8DebuggerClient* m_client;
};

} // namespace blink

#endif // !defined(InjectedScriptManager_h)
