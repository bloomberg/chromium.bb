// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLazyPropertyParser_h
#define CSSLazyPropertyParser_h

#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Heap.h"

namespace blink {

class CSSPropertyValueSet;

// Used for lazily parsing properties.
class CSSLazyPropertyParser
    : public GarbageCollectedFinalized<CSSLazyPropertyParser> {
 public:
  CSSLazyPropertyParser() = default;
  virtual ~CSSLazyPropertyParser() = default;
  virtual CSSPropertyValueSet* ParseProperties() = 0;
  virtual void Trace(blink::Visitor*) {}
  DISALLOW_COPY_AND_ASSIGN(CSSLazyPropertyParser);
};

}  // namespace blink

#endif  // CSSLazyPropertyParser_h
