// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYMORPHIC_CLASS_WITH_NON_VIRTUAL_TRACE_H_
#define POLYMORPHIC_CLASS_WITH_NON_VIRTUAL_TRACE_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject : public GarbageCollected<HeapObject> {
public:
    void trace(Visitor*) { }
};

class NonPolymorphicBase {
};

class PolymorphicBase {
public:
    virtual void foo();
};

class IsLeftMostPolymorphic
    : public GarbageCollected<IsLeftMostPolymorphic>,
      public PolymorphicBase {
public:
    void trace(Visitor*);
private:
    Member<HeapObject> m_obj;
};

class IsNotLeftMostPolymorphic
    : public GarbageCollected<IsNotLeftMostPolymorphic>,
      public NonPolymorphicBase,
      public PolymorphicBase {
public:
    void trace(Visitor*);
private:
    Member<HeapObject> m_obj;
};

}

#endif
