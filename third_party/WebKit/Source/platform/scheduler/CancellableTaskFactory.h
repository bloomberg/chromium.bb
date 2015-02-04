// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CancellableTaskFactory_h
#define CancellableTaskFactory_h

#include "platform/PlatformExport.h"
#include "public/platform/WebScheduler.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/WeakPtr.h"

namespace blink {
class TraceLocation;

class PLATFORM_EXPORT CancellableTaskFactory {
    WTF_MAKE_NONCOPYABLE(CancellableTaskFactory);

public:
    explicit CancellableTaskFactory(PassOwnPtr<Closure> closure)
        : m_closure(closure)
        , m_weakPtrFactory(this)
    {
    }

    bool isPending() const
    {
        return m_weakPtrFactory.hasWeakPtrs();
    }

    void cancel();

    // Returns a task that can be disabled by calling cancel().  The user takes
    // ownership of the task.  Creating a new task cancels any previous ones.
    WebThread::Task* task();

private:
    class CancellableTask : public WebThread::Task {
        WTF_MAKE_NONCOPYABLE(CancellableTask);

    public:
        explicit CancellableTask(WeakPtr<CancellableTaskFactory> weakPtr)
            : m_weakPtr(weakPtr) { }

        virtual ~CancellableTask() { }

        void run() override;

    private:
        WeakPtr<CancellableTaskFactory> m_weakPtr;
    };

    OwnPtr<Closure> m_closure;
    WeakPtrFactory<CancellableTaskFactory> m_weakPtrFactory;
};

} // namespace blink

#endif // CancellableTaskFactory_h
