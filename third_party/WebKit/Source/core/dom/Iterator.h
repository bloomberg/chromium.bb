// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Iterator_h
#define Iterator_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT Iterator : public GarbageCollectedFinalized<Iterator>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Iterator() {}
  virtual ~Iterator() {}

  virtual ScriptValue next(ScriptState*, ExceptionState&) = 0;
  virtual ScriptValue next(ScriptState*,
                           ScriptValue /* value */,
                           ExceptionState&) = 0;
  Iterator* GetIterator(ScriptState*, ExceptionState&) { return this; }

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // Iterator_h
