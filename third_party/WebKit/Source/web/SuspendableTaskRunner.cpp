// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/SuspendableTaskRunner.h"

#include "core/dom/ExecutionContext.h"

namespace blink {

void SuspendableTaskRunner::createAndRun(ExecutionContext* context, PassOwnPtr<WebThread::Task> task)
{
    RefPtrWillBeRawPtr<SuspendableTaskRunner> runner = adoptRefWillBeNoop(new SuspendableTaskRunner(context, task));
    runner->ref();
    runner->run();
}

void SuspendableTaskRunner::contextDestroyed()
{
    // This method can only be called if the script was not called in run()
    // and context remained suspend (method resume has never called).
    SuspendableTimer::contextDestroyed();
    deref();
}

SuspendableTaskRunner::SuspendableTaskRunner(ExecutionContext* context, PassOwnPtr<WebThread::Task> task)
    : SuspendableTimer(context)
    , m_task(task)
{
}

SuspendableTaskRunner::~SuspendableTaskRunner()
{
}

void SuspendableTaskRunner::fired()
{
    runAndDestroySelf();
}

void SuspendableTaskRunner::run()
{
    ExecutionContext* context = executionContext();
    ASSERT(context);
    if (!context->activeDOMObjectsAreSuspended()) {
        suspendIfNeeded();
        runAndDestroySelf();
        return;
    }
    startOneShot(0, FROM_HERE);
    suspendIfNeeded();
}

void SuspendableTaskRunner::runAndDestroySelf()
{
    // After calling the destructor of object - object will be unsubscribed from
    // resumed and contextDestroyed LifecycleObserver methods.
    m_task->run();
    deref();
}

DEFINE_TRACE(SuspendableTaskRunner)
{
#if ENABLE(OILPAN)
    visitor->trace(m_frame);
    visitor->trace(m_sources);
#endif
    SuspendableTimer::trace(visitor);
}

} // namespace blink
