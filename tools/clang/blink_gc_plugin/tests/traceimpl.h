// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRACEIMPL_H_
#define TRACEIMPL_H_

#include "heap/stubs.h"

namespace blink {

class X : public GarbageCollected<X> {
 public:
  virtual void trace(Visitor*) {}
};

class TraceImplInlined : public GarbageCollected<TraceImplInlined> {
 public:
  void trace(Visitor* visitor) { traceImpl(visitor); }

  template <typename VisitorDispatcher>
  void traceImpl(VisitorDispatcher visitor) {
    visitor->trace(m_x);
  }

 private:
  Member<X> m_x;
};

class TraceImplExtern : public GarbageCollected<TraceImplExtern> {
 public:
  void trace(Visitor* visitor);
  template <typename VisitorDispatcher>
  inline void traceImpl(VisitorDispatcher);

 private:
  Member<X> m_x;
};
}

#endif
