// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ProfilerAgentImpl_h
#define V8ProfilerAgentImpl_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/v8/public/V8ProfilerAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace v8 {
class Isolate;
}

namespace blink {

class V8DebuggerImpl;

class CORE_EXPORT V8ProfilerAgentImpl : public V8ProfilerAgent {
    WTF_MAKE_NONCOPYABLE(V8ProfilerAgentImpl);
public:
    explicit V8ProfilerAgentImpl(V8Debugger*);
    ~V8ProfilerAgentImpl() override;

    void setInspectorState(PassRefPtr<JSONObject> state) override { m_state = state; }
    void setFrontend(InspectorFrontend::Profiler* frontend) override { m_frontend = frontend; }
    void clearFrontend() override;
    void restore() override;

    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setSamplingInterval(ErrorString*, int) override;
    void start(ErrorString*) override;
    void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>&) override;

    void consoleProfile(const String& title) override;
    void consoleProfileEnd(const String& title) override;

    void idleStarted();
    void idleFinished();

private:
    void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>*);
    String nextProfileId();

    void startProfiling(const String& title);
    PassRefPtr<TypeBuilder::Profiler::CPUProfile> stopProfiling(const String& title, bool serialize);

    bool isRecording() const;

    V8DebuggerImpl* m_debugger;
    v8::Isolate* m_isolate;
    RefPtr<JSONObject> m_state;
    InspectorFrontend::Profiler* m_frontend;
    bool m_enabled;
    bool m_recordingCPUProfile;
    class ProfileDescriptor;
    Vector<ProfileDescriptor> m_startedProfiles;
    String m_frontendInitiatedProfileId;
};

} // namespace blink


#endif // !defined(V8ProfilerAgentImpl_h)
