// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STACK_ALLOCATED_H_
#define STACK_ALLOCATED_H_

#include "heap/stubs.h"

namespace WebCore {

class HeapObject : public GarbageCollected<HeapObject> { };

class PartObject {
    DISALLOW_ALLOCATION();
private:
    Member<HeapObject> m_obj; // Needs tracing.
};

class StackObject {
    STACK_ALLOCATED();
private:
    HeapObject* m_obj; // Does not need tracing.
};

}

#endif
