// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_STUBS_H_
#define HEAP_STUBS_H_

#include "stddef.h"

namespace WTF {

template<typename T> class RefCounted { };

template<typename T> class RawPtr {
public:
    operator T*() const { return 0; }
};

template<typename T> class RefPtr {
public:
    operator T*() const { return 0; }
};

template<typename T> class OwnPtr {
public:
    operator T*() const { return 0; }
};

class DefaultAllocator { };

template<typename T,
         size_t inlineCapacity = 0,
         typename Allocator = DefaultAllocator>
class Vector { };

}

namespace WebCore {

using namespace WTF;

#define DISALLOW_ALLOCATION()                   \
    private:                                    \
    void* operator new(size_t) = delete;

#define STACK_ALLOCATED()                           \
    private:                                        \
    __attribute__((annotate("blink_stack_allocated")))    \
    void* operator new(size_t) = delete;

#define NO_TRACE_CHECKING(bug)                      \
    __attribute__((annotate("blink_no_trace_checking")))

#define USING_GARBAGE_COLLECTED_MIXIN(type)             \
    public:                                             \
    virtual void adjustAndMark(Visitor*) const {}       \
    virtual bool isAlive(Visitor*) const { return 0; }

template<typename T> class GarbageCollected { };

template<typename T>
class GarbageCollectedFinalized : public GarbageCollected<T> { };

template<typename T> class Member {
public:
    operator T*() const { return 0; }
};

template<typename T> class Persistent {
public:
    operator T*() const { return 0; }
};

class HeapAllocator { };

template<typename T>
class HeapVector : public Vector<T, 0, HeapAllocator> { };

template<typename T>
class PersistentHeapVector : public Vector<T, 0, HeapAllocator> { };

class Visitor {
public:
    template<typename T> void trace(const T&);
};

class GarbageCollectedMixin {
    virtual void adjustAndMark(Visitor*) const = 0;
    virtual bool isAlive(Visitor*) const = 0;
};

}

#endif
