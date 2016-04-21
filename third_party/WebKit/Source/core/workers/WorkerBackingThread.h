// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerBackingThread_h
#define WorkerBackingThread_h

#include "core/CoreExport.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadingPrimitives.h"
#include <v8.h>

namespace blink {

class WebThread;
class WebThreadSupportingGC;

// WorkerBackingThread represents a WebThread with Oilpan and V8 potentially
// shared by multiple WebWorker scripts. A WebWorker needs to call attach() when
// attaching itself to the backing thread, and call detach() when the script
// no longer needs the thread.
// A WorkerBackingThread represents a WebThread while a WorkerThread corresponds
// to a web worker. There is one-to-one correspondence between WorkerThread and
// WorkerGlobalScope, but multiple WorkerThreads (i.e., multiple
// WorkerGlobalScopes) can share one WorkerBackingThread.
class CORE_EXPORT WorkerBackingThread final {
public:
    static PassOwnPtr<WorkerBackingThread> create(const char* name) { return adoptPtr(new WorkerBackingThread(name, false)); }
    static PassOwnPtr<WorkerBackingThread> create(WebThread* thread) { return adoptPtr(new WorkerBackingThread(thread, false)); }

    // These are needed to suppress leak reports. See
    // https://crbug.com/590802 and https://crbug.com/v8/1428.
    static PassOwnPtr<WorkerBackingThread> createForTest(const char* name) { return adoptPtr(new WorkerBackingThread(name, true)); }
    static PassOwnPtr<WorkerBackingThread> createForTest(WebThread* thread) { return adoptPtr(new WorkerBackingThread(thread, true)); }

    ~WorkerBackingThread();

    // attach() and detach() attaches and detaches Oilpan and V8 to / from
    // the caller worker script, respectively. attach() and detach() increments
    // and decrements m_workerScriptCount. attach() initializes Oilpan and V8
    // when m_workerScriptCount is 0. detach() terminates Oilpan and V8 when
    // m_workerScriptCount is 1. A worker script must not call any function
    // after calling detach().
    // They should be called from |this| thread.
    void attach();
    void detach();

    unsigned workerScriptCount()
    {
        MutexLocker locker(m_mutex);
        return m_workerScriptCount;
    }

    WebThreadSupportingGC& backingThread()
    {
        DCHECK(m_backingThread);
        return *m_backingThread;
    }

    v8::Isolate* isolate() { return m_isolate; }

private:
    WorkerBackingThread(const char* name, bool shouldCallGCOnShutdown);
    WorkerBackingThread(WebThread*, bool shouldCallGCOnSHutdown);
    void initialize();
    void shutdown();

    // Protects |m_workerScriptCount|.
    Mutex m_mutex;
    OwnPtr<WebThreadSupportingGC> m_backingThread;
    v8::Isolate* m_isolate = nullptr;
    unsigned m_workerScriptCount = 0;
    bool m_isOwningThread;
    bool m_shouldCallGCOnShutdown;
};

} // namespace blink

#endif // WorkerBackingThread_h
