// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_STUBS_H_
#define HEAP_STUBS_H_

#include "stddef.h"

namespace WebCore {

#define DISALLOW_ALLOCATION()                   \
    private:                                    \
    void* operator new(size_t) = delete;

#define STACK_ALLOCATED()                           \
    private:                                        \
    __attribute__((annotate("stack_allocated")))    \
    void* operator new(size_t) = delete;

#define NO_TRACE_CHECKING(bug)                      \
    __attribute__((annotate("no_trace_checking")))

template<typename T> class GarbageCollected { };
template<typename T> class Member { };

class Visitor {
public:
    template<typename T> void trace(const T&);
};

}

#endif
