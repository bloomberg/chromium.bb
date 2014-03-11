// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PURE_VIRTUAL_BASE_H_
#define PURE_VIRTUAL_BASE_H_

#include "heap/stubs.h"

namespace WebCore {

class A : public GarbageCollected<A> {
public:
    virtual void trace(Visitor*) = 0;
};

class B : public A {
public:
    // Does not need a trace method.
};

class C : public B {
public:
    void trace(Visitor*);
private:
    Member<A> m_a;
};

}

#endif
