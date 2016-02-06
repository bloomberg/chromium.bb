// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ProfilerAgent_h
#define V8ProfilerAgent_h

#include "core/CoreExport.h"
#include "core/InspectorBackendDispatcher.h"
#include "core/inspector/v8/V8Debugger.h"

namespace blink {

class CORE_EXPORT V8ProfilerAgent : public InspectorBackendDispatcher::ProfilerCommandHandler, public V8Debugger::Agent<InspectorFrontend::Profiler> {
public:
    static PassOwnPtr<V8ProfilerAgent> create(V8Debugger*);
    virtual ~V8ProfilerAgent() { }

    // API for the embedder.
    virtual void consoleProfile(const String& title) = 0;
    virtual void consoleProfileEnd(const String& title) = 0;

    virtual void idleStarted() = 0;
    virtual void idleFinished() = 0;
};

} // namespace blink


#endif // !defined(V8ProfilerAgent_h)
