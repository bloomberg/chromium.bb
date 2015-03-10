// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuspendableTaskRunner_h
#define SuspendableTaskRunner_h

#include "core/frame/SuspendableTimer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebThread.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ExecutionContext;

class SuspendableTaskRunner final : public RefCountedWillBeRefCountedGarbageCollected<SuspendableTaskRunner>, public SuspendableTimer {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(SuspendableTaskRunner);
public:
    static void createAndRun(ExecutionContext*, PassOwnPtr<WebThread::Task>);
    virtual ~SuspendableTaskRunner();

    void contextDestroyed() override;

    DECLARE_VIRTUAL_TRACE();

private:
    SuspendableTaskRunner(ExecutionContext*, PassOwnPtr<WebThread::Task>);

    void fired() override;

    void run();
    void runAndDestroySelf();

    OwnPtr<WebThread::Task> m_task;
};

} // namespace blink

#endif // SuspendableTaskRunner_h
