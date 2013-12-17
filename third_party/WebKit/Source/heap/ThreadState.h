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

#ifndef ThreadState_h
#define ThreadState_h

#include "wtf/HashSet.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"

namespace WebCore {

class ThreadState {
public:
    // Initialize threading infrastructure. Should be called from the main
    // thread.
    static void init(intptr_t* startOfStack);
    static void shutdown();

    // Associate ThreadState object with the current thread. After this
    // call thread can start using the garbage collected heap infrastructure.
    // It also has to periodically check for safepoints.
    static void attach(intptr_t* startOfStack);

    // Disassociate attached ThreadState from the current thread. The thread
    // can no longer use the garbage collected heap after this call.
    static void detach();

    static ThreadState* current() { return **s_threadSpecific; }
    static ThreadState* mainThreadState()
    {
        return reinterpret_cast<ThreadState*>(s_mainThreadStateStorage);
    }

    static bool isMainThread() { return current() == mainThreadState(); }

    inline void checkThread() const
    {
        ASSERT(m_thread == currentThread());
    }

private:
    explicit ThreadState(intptr_t* startOfStack);
    ~ThreadState();

    typedef HashSet<ThreadState*> AttachedThreadStateSet;
    static AttachedThreadStateSet& attachedThreads();

    static WTF::ThreadSpecific<ThreadState*>* s_threadSpecific;

    // We can't create a static member of type ThreadState here
    // because it will introduce global constructor and destructor.
    // We would like to manage lifetime of the ThreadState attached
    // to the main thread explicitly instead and still use normal
    // constructor and destructor for the ThreadState class.
    // For this we reserve static storage for the main ThreadState
    // and lazily construct ThreadState in it using placement new.
    static uint8_t s_mainThreadStateStorage[];

    ThreadIdentifier m_thread;
    intptr_t* m_startOfStack;
};

}

#endif // ThreadState_h
