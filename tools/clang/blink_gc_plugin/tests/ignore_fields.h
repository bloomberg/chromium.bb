// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IGNORE_FIELDS_H_
#define IGNORE_FIELDS_H_

#include "heap/stubs.h"

namespace WebCore {

class HeapObject : public GarbageCollected<HeapObject> { };

class A {
private:
    NO_TRACE_CHECKING("http://crbug.com/12345")
    HeapObject* m_obj;
};

}

#endif
