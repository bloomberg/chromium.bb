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
    T* operator->() { return 0; }
};

template<typename T> class RefPtr {
public:
    ~RefPtr() { }
    operator T*() const { return 0; }
    T* operator->() { return 0; }
};

template<typename T> class OwnPtr {
public:
    ~OwnPtr() { }
    operator T*() const { return 0; }
    T* operator->() { return 0; }
};

class DefaultAllocator {
public:
    static const bool isGarbageCollected = false;
};

template<typename T>
struct VectorTraits {
    static const bool needsDestruction = true;
};

template<size_t inlineCapacity, bool isGarbageCollected, bool tNeedsDestruction>
class VectorDestructorBase {
public:
    ~VectorDestructorBase() {}
};

template<size_t inlineCapacity>
class VectorDestructorBase<inlineCapacity, true, false> {};

template<>
class VectorDestructorBase<0, true, true> {};

template<typename T,
         size_t inlineCapacity = 0,
         typename Allocator = DefaultAllocator>
class Vector : public VectorDestructorBase<inlineCapacity,
                                           Allocator::isGarbageCollected,
                                           VectorTraits<T>::needsDestruction> {
public:
    size_t size();
    T& operator[](size_t);
};

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
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

template<typename T> class Persistent {
public:
    operator T*() const { return 0; }
    T* operator->() { return 0; }
    bool operator!() const { return false; }
};

class HeapAllocator {
public:
    static const bool isGarbageCollected = true;
};

template<typename T, size_t inlineCapacity = 0>
class HeapVector : public Vector<T, inlineCapacity, HeapAllocator> { };

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

namespace WTF {

template<typename T>
struct VectorTraits<WebCore::Member<T> > {
    static const bool needsDestruction = false;
};

}

#endif
