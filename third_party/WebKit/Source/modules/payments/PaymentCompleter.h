// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentCompleter_h
#define PaymentCompleter_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT PaymentCompleter : public GarbageCollectedMixin {
 public:
  enum PaymentComplete { kFail, kSuccess, kUnknown };

  virtual ScriptPromise Complete(ScriptState*, PaymentComplete result) = 0;

 protected:
  virtual ~PaymentCompleter() {}
};

}  // namespace blink

#endif  // PaymentCompleter_h
