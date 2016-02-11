// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HeapProfilerAgent_h
#define V8HeapProfilerAgent_h

#include "core/CoreExport.h"
#include "core/InspectorBackendDispatcher.h"
#include "core/inspector/v8/public/V8Debugger.h"

namespace blink {

class V8RuntimeAgent;

class CORE_EXPORT V8HeapProfilerAgent : public InspectorBackendDispatcher::HeapProfilerCommandHandler, public V8Debugger::Agent<InspectorFrontend::HeapProfiler> {
public:
    static PassOwnPtr<V8HeapProfilerAgent> create(v8::Isolate*, V8RuntimeAgent*);
    virtual ~V8HeapProfilerAgent() { }

    virtual void requestHeapStatsUpdate() = 0;
};

} // namespace blink

#endif // !defined(V8HeapProfilerAgent_h)
