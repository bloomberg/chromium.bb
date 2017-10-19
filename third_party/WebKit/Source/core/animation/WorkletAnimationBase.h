// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletAnimationBase_h
#define WorkletAnimationBase_h

#include "core/CoreExport.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class CORE_EXPORT WorkletAnimationBase
    : public GarbageCollectedFinalized<WorkletAnimationBase> {
 public:
  virtual ~WorkletAnimationBase() {}

  // Attempts to start the animation on the compositor side, returning true if
  // it succeeds or false otherwise.
  //
  // On a false return it may still be possible to start the animation on the
  // compositor later (e.g. if an incompatible property is removed from the
  // element), so the caller should try again next main frame.
  virtual bool StartOnCompositor() = 0;

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // WorkletAnimationBase_h
