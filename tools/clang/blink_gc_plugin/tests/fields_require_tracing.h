// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIELDS_REQUIRE_TRACING_H_
#define FIELDS_REQUIRE_TRACING_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject;

class PartObject {
    DISALLOW_ALLOCATION();
public:
    void trace(Visitor*);
private:
    Member<HeapObject> m_obj1;
    Member<HeapObject> m_obj2;
    Member<HeapObject> m_obj3;
};

class HeapObject : public GarbageCollected<HeapObject> {
public:
    void trace(Visitor*);
private:
    PartObject m_part;
    Member<HeapObject> m_obj;
};

}

#endif
