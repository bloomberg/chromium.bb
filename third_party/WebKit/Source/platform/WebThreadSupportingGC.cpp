// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/WebThreadSupportingGC.h"

namespace blink {

PassOwnPtr<WebThreadSupportingGC> WebThreadSupportingGC::create(const char* name)
{
    return adoptPtr(new WebThreadSupportingGC(name));
}

WebThreadSupportingGC::WebThreadSupportingGC(const char* name)
    : m_thread(adoptPtr(blink::Platform::current()->createThread(name)))
{
}

WebThreadSupportingGC::~WebThreadSupportingGC()
{
    if (ThreadState::current()) {
        // WebThread's destructor blocks until all the tasks are processed.
        ThreadState::SafePointScope scope(ThreadState::HeapPointersOnStack);
        m_thread.clear();
    }
}

void WebThreadSupportingGC::attachGC()
{
    m_pendingGCRunner = adoptPtr(new PendingGCRunner);
    m_messageLoopInterruptor = adoptPtr(new MessageLoopInterruptor(&platformThread()));
    platformThread().addTaskObserver(m_pendingGCRunner.get());
    ThreadState::attach();
    ThreadState::current()->addInterruptor(m_messageLoopInterruptor.get());
}

void WebThreadSupportingGC::detachGC()
{
    ThreadState::current()->removeInterruptor(m_messageLoopInterruptor.get());
    ThreadState::detach();
    platformThread().removeTaskObserver(m_pendingGCRunner.get());
    m_pendingGCRunner = nullptr;
    m_messageLoopInterruptor = nullptr;
}

} // namespace blink
