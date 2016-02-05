// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ProfilerAgent_h
#define V8ProfilerAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class JSONObject;
class V8Debugger;

typedef String ErrorString;

class CORE_EXPORT V8ProfilerAgent {
public:
    static PassOwnPtr<V8ProfilerAgent> create(V8Debugger*);
    virtual ~V8ProfilerAgent() { }

    // State management methods.
    virtual void setInspectorState(PassRefPtr<JSONObject>) = 0;
    virtual void setFrontend(InspectorFrontend::Profiler*) = 0;
    virtual void clearFrontend() = 0;
    virtual void restore() = 0;

    // Protocol methods.
    virtual void enable(ErrorString*) = 0;
    virtual void disable(ErrorString*) = 0;
    virtual void setSamplingInterval(ErrorString*, int) = 0;
    virtual void start(ErrorString*) = 0;
    virtual void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>&) = 0;

    // API for the embedder.
    virtual void consoleProfile(const String& title) = 0;
    virtual void consoleProfileEnd(const String& title) = 0;

    virtual void idleStarted() = 0;
    virtual void idleFinished() = 0;
};

} // namespace blink


#endif // !defined(V8ProfilerAgent_h)
