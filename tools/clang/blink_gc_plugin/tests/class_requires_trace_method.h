// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLASS_REQUIRES_TRACE_METHOD_H_
#define CLASS_REQUIRES_TRACE_METHOD_H_

#include "heap/stubs.h"

namespace blink {

class HeapObject;

class PartObject {
    DISALLOW_ALLOCATION();
private:
    Member<HeapObject> m_obj;
};

class HeapObject : public GarbageCollected<HeapObject> {
private:
    PartObject m_part;
};

}

#endif
