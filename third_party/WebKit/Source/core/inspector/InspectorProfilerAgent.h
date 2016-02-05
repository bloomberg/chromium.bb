/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorProfilerAgent_h
#define InspectorProfilerAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class InspectorFrontend;
class V8Debugger;
class V8ProfilerAgent;

typedef String ErrorString;

class CORE_EXPORT InspectorProfilerAgent final : public InspectorBaseAgent<InspectorProfilerAgent, InspectorFrontend::Profiler>, public InspectorBackendDispatcher::ProfilerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent);
    USING_FAST_MALLOC_WILL_BE_REMOVED(InspectorProfilerAgent);
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void profilingStarted() { }
        virtual void profilingStopped() { }
    };

    static PassOwnPtrWillBeRawPtr<InspectorProfilerAgent> create(V8Debugger*, Client*);
    ~InspectorProfilerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    // InspectorBaseAgent overrides.
    void setState(PassRefPtr<JSONObject>) override;
    void setFrontend(InspectorFrontend*) override;
    void clearFrontend() override;
    void restore() override;

    void consoleProfile(ExecutionContext*, const String& title);
    void consoleProfileEnd(const String& title);

    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setSamplingInterval(ErrorString*, int) override;
    void start(ErrorString*) override;
    void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>&) override;

    void willProcessTask();
    void didProcessTask();
    void willEnterNestedRunLoop();
    void didLeaveNestedRunLoop();

private:
    InspectorProfilerAgent(V8Debugger*, Client*);

    Client* m_client;
    OwnPtr<V8ProfilerAgent> m_v8ProfilerAgent;
};

} // namespace blink


#endif // !defined(InspectorProfilerAgent_h)
