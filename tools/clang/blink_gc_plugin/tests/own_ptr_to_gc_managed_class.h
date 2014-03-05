// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OWN_PTR_TO_GC_MANAGED_CLASS_H_
#define OWN_PTR_TO_GC_MANAGED_CLASS_H_

#include "heap/stubs.h"

namespace WebCore {

class HeapObject;

class PartObject {
    DISALLOW_ALLOCATION();
private:
    OwnPtr<HeapObject> m_obj;
};

class HeapObject : public GarbageCollected<HeapObject> {
public:
    void trace(Visitor*);
private:
    Vector<OwnPtr<HeapObject> > m_objs;
};

}

#endif
