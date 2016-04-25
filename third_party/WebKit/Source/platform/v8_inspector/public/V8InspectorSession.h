// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSession_h
#define V8InspectorSession_h

#include "platform/PlatformExport.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class V8DebuggerAgent;
class V8HeapProfilerAgent;
class V8InspectorSessionClient;
class V8ProfilerAgent;
class V8RuntimeAgent;

class PLATFORM_EXPORT V8InspectorSession {
public:
    virtual ~V8InspectorSession() { }

    virtual void setClient(V8InspectorSessionClient*) = 0;

    virtual V8DebuggerAgent* debuggerAgent() = 0;
    virtual V8HeapProfilerAgent* heapProfilerAgent() = 0;
    virtual V8ProfilerAgent* profilerAgent() = 0;
    virtual V8RuntimeAgent* runtimeAgent() = 0;
};

} // namespace blink

#endif // V8InspectorSession_h
