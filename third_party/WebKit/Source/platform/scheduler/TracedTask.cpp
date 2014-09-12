// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/TracedTask.h"

namespace blink {

volatile int TracedTask::s_nextFlowTraceID = 0;

void TracedTask::run() const
{
    TRACE_EVENT_FLOW_END0("blink", m_traceName, MANGLE(m_flowTraceID));

    TRACE_EVENT2("blink", m_traceName,
        "src_file", m_location.fileName(),
        "src_func", m_location.functionName());

    m_task();
}

TracedTask::TracedTask(const Task& task, const TraceLocation& location, const char* traceName)
    : m_task(task)
    , m_location(location)
    , m_traceName(traceName)
{
    bool tracingEnabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink", &tracingEnabled);

    if (tracingEnabled) {
        // atomicIncrement is slow so we only do it if tracing is enabled
        m_flowTraceID = static_cast<uint64_t>(atomicIncrement(&s_nextFlowTraceID));

        TRACE_EVENT_FLOW_BEGIN2("blink", m_traceName, MANGLE(m_flowTraceID),
        "src_file", m_location.fileName(),
        "src_func", m_location.functionName());
    }
}

} // namespace blink
