// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/TracedTask.h"
#include "platform/scheduler/Scheduler.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
namespace internal {

volatile int TracedTask::s_nextFlowTraceID = 0;

TracedTask::TracedTask(const TraceLocation& location, const char* traceName)
    : m_location(location)
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

TraceLocation TracedTask::getLocation() const
{
    return m_location;
}

const char* TracedTask::getTraceName() const
{
    return m_traceName;
}

void TracedTask::endFlowTraceEvent() const
{
    TRACE_EVENT_FLOW_END0("blink", m_traceName, MANGLE(m_flowTraceID));
}

TracedTask::~TracedTask() { }

TracedStandardTask::TracedStandardTask(const Task& task, const TraceLocation& location, const char* traceName)
    : TracedTask(location, traceName)
    , m_task(task) { }

TracedStandardTask::~TracedStandardTask() { }

// static
PassOwnPtr<TracedStandardTask> TracedStandardTask::Create(const Task& task, const TraceLocation& location, const char* traceName)
{
    return adoptPtr(new TracedStandardTask(task, location, traceName));
}

void TracedStandardTask::run() const
{
    endFlowTraceEvent();
    TRACE_EVENT2("blink", getTraceName(),
        "src_file", getLocation().fileName(),
        "src_func", getLocation().functionName());

    m_task();
}

TracedIdleTask::TracedIdleTask(const IdleTask& idleTask, const TraceLocation& location, const char* traceName)
    : TracedTask(location, traceName)
    , m_idleTask(idleTask)
{
    ASSERT(Scheduler::shared());
}

TracedIdleTask::~TracedIdleTask() { }

// static
PassOwnPtr<TracedIdleTask> TracedIdleTask::Create(const IdleTask& idleTask, const TraceLocation& location, const char* traceName)
{
    return adoptPtr(new TracedIdleTask(idleTask, location, traceName));
}

void TracedIdleTask::run() const
{
    endFlowTraceEvent();
    TRACE_EVENT2("blink", getTraceName(),
        "src_file", getLocation().fileName(),
        "src_func", getLocation().functionName());

    m_idleTask(Scheduler::shared()->currentFrameDeadlineForIdleTasks());
}

} // namespace internal
} // namespace blink
