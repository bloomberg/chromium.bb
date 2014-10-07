// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TracedTask_h
#define TracedTask_h

#include "platform/Task.h"
#include "platform/TraceEvent.h"
#include "platform/TraceLocation.h"

namespace blink {
namespace internal {

#ifdef MANGLE_COMPILES_ON_WIN_OK
// TODO: Once win version compiles correctly when using TRACE_ID_MANGLE remove
// the ifdef to always mangle
#define MANGLE(id) TRACE_ID_MANGLE(id)
#else
#define MANGLE(id) (id)
#endif

class TracedTask {
public:
    virtual void run() const = 0;
    virtual ~TracedTask();

protected:
    TracedTask(const TraceLocation&, const char* traceName);
    TraceLocation getLocation() const;
    const char* getTraceName() const;
    void endFlowTraceEvent() const;

private:
    // Declared volatile as it is atomically incremented.
    static volatile int s_nextFlowTraceID;

    uint64_t m_flowTraceID;
    TraceLocation m_location;
    const char* m_traceName;
};

class TracedStandardTask : public TracedTask {
public:
    typedef Function<void()> Task;

    static PassOwnPtr<TracedStandardTask> Create(const Task&, const TraceLocation&, const char* traceName);
    virtual void run() const;
    virtual ~TracedStandardTask();

private:
    TracedStandardTask(const Task&, const TraceLocation&, const char* traceName);
    Task m_task;
};

class TracedIdleTask : public TracedTask {
public:
    typedef Function<void(double deadlineSeconds)> IdleTask;

    static PassOwnPtr<TracedIdleTask> Create(const IdleTask&, const TraceLocation&, const char* traceName);
    virtual void run() const;
    virtual ~TracedIdleTask();

private:
    TracedIdleTask(const IdleTask&, const TraceLocation&, const char* traceName);
    IdleTask m_idleTask;
};

} // namespace internal
} // namespace blink

#endif // TracedTask_h
