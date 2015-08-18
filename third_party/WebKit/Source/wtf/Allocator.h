// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_Allocator_h
#define WTF_Allocator_h

#include "wtf/StdLibExtras.h"

namespace WTF {

// Classes that contain references to garbage-collected objects but aren't
// themselves garbaged allocated, have some extra macros available which
// allows their use to be restricted to cases where the garbage collector
// is able to discover their references. These macros will be useful for
// non-garbage-collected objects to avoid unintended allocations.
//
// STACK_ALLOCATED(): Use if the object is only stack allocated.
// Garbage-collected objects should be in Members but you do not need the
// trace method as they are on the stack.  (Down the line these might turn
// in to raw pointers, but for now Members indicate that we have thought
// about them and explicitly taken care of them.)
//
// DISALLOW_ALLOCATION(): Cannot be allocated with new operators but can be a
// part of object.  If it has Members you need a trace method and the containing
// object needs to call that trace method.
//
// ALLOW_ONLY_INLINE_ALLOCATION(): Allows only placement new operator.  This
// disallows general allocation of this object but allows to put the object as a
// value object in collections.  If these have Members you need to have a trace
// method. That trace method will be called automatically by the on-heap
// collections.
//
#define DISALLOW_ALLOCATION()                                   \
    private:                                                    \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:

#define ALLOW_ONLY_INLINE_ALLOCATION()                                              \
    public:                                                                         \
        using IsAllowOnlyInlineAllocation = int;                                    \
        void* operator new(size_t, NotNullTag, void* location) { return location; } \
        void* operator new(size_t, void* location) { return location; }             \
    private:                                                                        \
        void* operator new(size_t) = delete;                                        \
    public:

#define STATIC_ONLY(Type) \
    private:              \
        Type() = delete;  \
        Type(const Type&) = delete;                             \
        Type& operator=(const Type&) = delete;                  \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:

#if COMPILER(CLANG)
#define STACK_ALLOCATED()                                       \
    private:                                                    \
        __attribute__((annotate("blink_stack_allocated")))      \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTag, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:
#else
#define STACK_ALLOCATED() DISALLOW_ALLOCATION()
#endif

} // namespace WTF

#endif /* WTF_Allocator_h */
