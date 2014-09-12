// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedTask_h
#define TracedTask_h

#include "platform/Task.h"
#include "platform/TraceEvent.h"
#include "platform/TraceLocation.h"

namespace blink {

#ifdef MANGLE_COMPILES_ON_WIN_OK
// TODO: Once win version compiles correctly when using TRACE_ID_MANGLE remove
// the ifdef to always mangle
#define MANGLE(id) TRACE_ID_MANGLE(id)
#else
#define MANGLE(id) (id)
#endif

class TracedTask {
public:
    typedef Function<void()> Task;

    void run() const;

private:
    friend class Scheduler;
    TracedTask(const Task&, const TraceLocation&, const char* traceName);

    // Declared volatile as it is atomically incremented.
    static volatile int s_nextFlowTraceID;

    uint64_t m_flowTraceID;
    Task m_task;
    TraceLocation m_location;
    const char* m_traceName;
};

} // namespace blink

#endif // TracedTask_h
