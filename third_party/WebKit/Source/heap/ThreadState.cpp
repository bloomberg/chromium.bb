/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "heap/ThreadState.h"

#include "heap/Heap.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {

WTF::ThreadSpecific<ThreadState*>* ThreadState::s_threadSpecific = 0;
uint8_t ThreadState::s_mainThreadStateStorage[sizeof(ThreadState)];

static Mutex& threadAttachMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

ThreadState::ThreadState(intptr_t* startOfStack)
    : m_thread(currentThread())
    , m_startOfStack(startOfStack)
{
    ASSERT(!**s_threadSpecific);
    **s_threadSpecific = this;

    // FIXME: This is to silence clang that complains about unused private
    // member. Remove once we implement stack scanning that uses it.
    (void) m_startOfStack;
}

ThreadState::~ThreadState()
{
    checkThread();
    **s_threadSpecific = 0;
}

void ThreadState::init(intptr_t* startOfStack)
{
    s_threadSpecific = new WTF::ThreadSpecific<ThreadState*>();
    new(s_mainThreadStateStorage) ThreadState(startOfStack);
    attachedThreads().add(mainThreadState());
}

void ThreadState::shutdown()
{
    mainThreadState()->~ThreadState();
}

void ThreadState::attach(intptr_t* startOfStack)
{
    MutexLocker locker(threadAttachMutex());
    ThreadState* state = new ThreadState(startOfStack);
    attachedThreads().add(state);
}

void ThreadState::detach()
{
    ThreadState* state = current();
    MutexLocker locker(threadAttachMutex());
    attachedThreads().remove(state);
    delete state;
}

// Trigger garbage collection on a 50% increase in size, but not for
// less than 2 pages.
static bool increasedEnoughToGC(size_t newSize, size_t oldSize)
{
    if (newSize < 2 * blinkPagePayloadSize())
        return false;
    return newSize > oldSize + (oldSize >> 1);
}

// FIXME: The heuristics are local for a thread at this
// point. Consider using heuristics that take memory for all threads
// into account.
bool ThreadState::shouldGC()
{
    return increasedEnoughToGC(m_stats.totalObjectSpace(), m_statsAfterLastGC.totalObjectSpace());
}

// Trigger conservative garbage collection on a 100% increase in size,
// but not for less than 2 pages.
static bool increasedEnoughToForceConservativeGC(size_t newSize, size_t oldSize)
{
    if (newSize < 2 * blinkPagePayloadSize())
        return false;
    return newSize > 2 * oldSize;
}

// FIXME: The heuristics are local for a thread at this
// point. Consider using heuristics that take memory for all threads
// into account.
bool ThreadState::shouldForceConservativeGC()
{
    return increasedEnoughToForceConservativeGC(m_stats.totalObjectSpace(), m_statsAfterLastGC.totalObjectSpace());
}

ThreadState::AttachedThreadStateSet& ThreadState::attachedThreads()
{
    DEFINE_STATIC_LOCAL(AttachedThreadStateSet, threads, ());
    return threads;
}

}
