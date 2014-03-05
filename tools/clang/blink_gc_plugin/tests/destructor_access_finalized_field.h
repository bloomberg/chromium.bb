// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DESTRUCTOR_ACCESS_FINALIZED_FIELD_H_
#define DESTRUCTOR_ACCESS_FINALIZED_FIELD_H_

#include "heap/stubs.h"

namespace WebCore {

class Other : public RefCounted<Other> {};

class HeapObject : public GarbageCollectedFinalized<HeapObject> {
public:
    ~HeapObject();
    void trace(Visitor*);
private:
    RefPtr<Other> m_ref;
    Member<HeapObject> m_obj;
    Vector<Member<HeapObject> > m_objs;
};

}

#endif
