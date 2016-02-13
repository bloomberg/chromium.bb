// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HeapProfilerAgent_h
#define V8HeapProfilerAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

class V8RuntimeAgent;

class PLATFORM_EXPORT V8HeapProfilerAgent : public protocol::Dispatcher::HeapProfilerCommandHandler, public V8Debugger::Agent<protocol::Frontend::HeapProfiler> {
public:
    static PassOwnPtr<V8HeapProfilerAgent> create(v8::Isolate*, V8RuntimeAgent*);
    virtual ~V8HeapProfilerAgent() { }

    virtual void requestHeapStatsUpdate() = 0;
};

} // namespace blink

#endif // !defined(V8HeapProfilerAgent_h)
