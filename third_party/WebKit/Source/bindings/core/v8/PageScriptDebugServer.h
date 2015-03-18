/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#ifndef PageScriptDebugServer_h
#define PageScriptDebugServer_h

#include "bindings/core/v8/ScriptDebugServer.h"
#include <v8.h>

namespace WTF {
class Mutex;
}

namespace blink {

class Page;

class PageScriptDebugServer final : public ScriptDebugServer {
    WTF_MAKE_NONCOPYABLE(PageScriptDebugServer);
public:
    class ClientMessageLoop {
    public:
        virtual ~ClientMessageLoop() { }
        virtual void run(LocalFrame*) = 0;
        virtual void quitNow() = 0;
    };

    PageScriptDebugServer(PassOwnPtr<ClientMessageLoop>, v8::Isolate*);
    ~PageScriptDebugServer() override;
    DECLARE_VIRTUAL_TRACE();

    void addListener(ScriptDebugListener*, LocalFrame*, int contextDebugId);
    void removeListener(ScriptDebugListener*, LocalFrame*);

    static PageScriptDebugServer* instance();
    static void interruptMainThreadAndRun(PassOwnPtr<Task>);

    void compileScript(ScriptState*, const String& expression, const String& sourceURL, bool persistScript, String* scriptId, String* exceptionDetailsText, int* lineNumber, int* columnNumber, RefPtrWillBeRawPtr<ScriptCallStack>* stackTrace) override;
    void clearCompiledScripts() override;
    void runScript(ScriptState*, const String& scriptId, ScriptValue* result, bool* wasThrown, String* exceptionDetailsText, int* lineNumber, int* columnNumber, RefPtrWillBeRawPtr<ScriptCallStack>* stackTrace) override;

    void muteWarningsAndDeprecations() override;
    void unmuteWarningsAndDeprecations() override;

private:
    ScriptDebugListener* getDebugListenerForContext(v8::Handle<v8::Context>) override;
    void runMessageLoopOnPause(v8::Handle<v8::Context>) override;
    void quitMessageLoopOnPause() override;
    static WTF::Mutex& creationMutex();

    using ListenersMap = WillBeHeapHashMap<RawPtrWillBeMember<LocalFrame>, ScriptDebugListener*>;
    ListenersMap m_listenersMap;
    OwnPtr<ClientMessageLoop> m_clientMessageLoop;
    RawPtrWillBeMember<LocalFrame> m_pausedFrame;
    HashMap<String, String> m_compiledScriptURLs;

    static PageScriptDebugServer* s_instance;
};

} // namespace blink


#endif // PageScriptDebugServer_h
