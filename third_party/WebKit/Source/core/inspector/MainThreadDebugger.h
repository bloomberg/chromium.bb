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

#ifndef MainThreadDebugger_h
#define MainThreadDebugger_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/heap/Handle.h"
#include <v8.h>

namespace WTF {
class Mutex;
}

namespace blink {

class LocalFrame;
class V8Debugger;

class CORE_EXPORT MainThreadDebugger final : public ThreadDebugger {
    WTF_MAKE_NONCOPYABLE(MainThreadDebugger);
public:
    class ClientMessageLoop {
        USING_FAST_MALLOC(ClientMessageLoop);
    public:
        virtual ~ClientMessageLoop() { }
        virtual void run(LocalFrame*) = 0;
        virtual void quitNow() = 0;
    };

    static PassOwnPtr<MainThreadDebugger> create(PassOwnPtr<ClientMessageLoop> clientMessageLoop, v8::Isolate* isolate)
    {
        return adoptPtr(new MainThreadDebugger(std::move(clientMessageLoop), isolate));
    }

    ~MainThreadDebugger() override;

    static void initializeContext(v8::Local<v8::Context>, LocalFrame*, int worldId);
    static int contextGroupId(LocalFrame*);

    static MainThreadDebugger* instance();
    static void interruptMainThreadAndRun(PassOwnPtr<InspectorTaskRunner::Task>);
    InspectorTaskRunner* taskRunner() const { return m_taskRunner.get(); }

private:
    MainThreadDebugger(PassOwnPtr<ClientMessageLoop>, v8::Isolate*);

    // V8DebuggerClient implementation.
    void runMessageLoopOnPause(int contextGroupId) override;
    void quitMessageLoopOnPause() override;
    bool canAccessContext(v8::Local<v8::Context>) override;

    static WTF::Mutex& creationMutex();

    OwnPtr<ClientMessageLoop> m_clientMessageLoop;
    OwnPtr<InspectorTaskRunner> m_taskRunner;

    static MainThreadDebugger* s_instance;
};

} // namespace blink


#endif // MainThreadDebugger_h
