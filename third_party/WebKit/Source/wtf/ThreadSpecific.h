/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Jian Li <jianli@chromium.org>
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Thread local storage is implemented by using either pthread API or Windows
 * native API. There is subtle semantic discrepancy for the cleanup function
 * implementation as noted below:
 *   @ In pthread implementation, the destructor function will be called
 *     repeatedly if there is still non-NULL value associated with the function.
 *   @ In Windows native implementation, the destructor function will be called
 *     only once.
 * This semantic discrepancy does not impose any problem because nowhere in
 * WebKit the repeated call bahavior is utilized.
 */

#ifndef WTF_ThreadSpecific_h
#define WTF_ThreadSpecific_h

#include "base/threading/thread_local_storage.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/Partitions.h"
#include "wtf/WTF.h"

namespace WTF {

template<typename T>
class ThreadSpecific {
    USING_FAST_MALLOC(ThreadSpecific);
    WTF_MAKE_NONCOPYABLE(ThreadSpecific);
public:
    ThreadSpecific()
        : m_slot(&destory)
    { }

    operator T*() { return get(); }
    T* operator->() { return get(); }
    T& operator*() { return *get(); }

private:
    // Not implemented. It's technically possible to destroy a thread specific key, but one would need
    // to make sure that all values have been destroyed already (usually, that all threads that used it
    // have exited). It's unlikely that any user of this call will be in that situation - and having
    // a destructor defined can be confusing, given that it has such strong pre-requisites to work correctly.
    ~ThreadSpecific();

    T* get()
    {
        T* ptr = static_cast<T*>(m_slot.Get());
        if (!ptr) {
            // Set up thread-specific value's memory pointer before invoking constructor, in case any function it calls
            // needs to access the value, to avoid recursion.
            ptr = static_cast<T*>(Partitions::fastZeroedMalloc(sizeof(T), WTF_HEAP_PROFILER_TYPE_NAME(T)));
            m_slot.Set(ptr);
            new (NotNull, ptr) T;
        }
        return ptr;
    }

    static void destory(void* value)
    {
        if (isShutdown())
            return;
        T* ptr = static_cast<T*>(value);
        ptr->~T();
        Partitions::fastFree(ptr);
    }

    base::ThreadLocalStorage::Slot m_slot;
};

} // namespace WTF

using WTF::ThreadSpecific;

#endif // WTF_ThreadSpecific_h
