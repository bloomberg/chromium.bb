// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/WebThreadSupportingGC.h"

#include "platform/heap/SafePoint.h"
#include "public/platform/WebScheduler.h"
#include "wtf/Threading.h"

namespace blink {

PassOwnPtr<WebThreadSupportingGC> WebThreadSupportingGC::create(const char* name)
{
#if ENABLE(ASSERT)
    WTF::willCreateThread();
#endif
    return adoptPtr(new WebThreadSupportingGC(name));
}

WebThreadSupportingGC::WebThreadSupportingGC(const char* name)
    : m_thread(adoptPtr(Platform::current()->createThread(name)))
{
}

WebThreadSupportingGC::~WebThreadSupportingGC()
{
    if (ThreadState::current()) {
        // WebThread's destructor blocks until all the tasks are processed.
        SafePointScope scope(ThreadState::HeapPointersOnStack);
        m_thread.clear();
    }
}

void WebThreadSupportingGC::initialize()
{
    m_pendingGCRunner = adoptPtr(new PendingGCRunner);
    platformThread().addTaskObserver(m_pendingGCRunner.get());
    ThreadState::attach();
    OwnPtr<MessageLoopInterruptor> interruptor = adoptPtr(new MessageLoopInterruptor(&platformThread()));
    ThreadState::current()->addInterruptor(interruptor.release());
}

void WebThreadSupportingGC::shutdown()
{
    // Ensure no posted tasks will run from this point on.
    platformThread().removeTaskObserver(m_pendingGCRunner.get());
    platformThread().scheduler()->shutdown();

    ThreadState::detach();
    m_pendingGCRunner = nullptr;
}

} // namespace blink
