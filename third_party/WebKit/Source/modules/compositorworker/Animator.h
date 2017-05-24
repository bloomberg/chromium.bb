// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Animator_h
#define Animator_h

#include "platform/bindings/ScopedPersistent.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"

namespace blink {

class AnimatorDefinition;

class Animator final : public GarbageCollectedFinalized<Animator> {
 public:
  Animator(v8::Isolate*, AnimatorDefinition*, v8::Local<v8::Object> instance);
  ~Animator();
  DECLARE_TRACE();

 private:
  // This object keeps the definition object, and animator instance alive.
  // It needs to be destroyed to break a reference cycle between it and the
  // AnimationWorkletGlobalScope. The reference cycle is broken at
  // |AnimationWorkletGlobalScope::Dispose()|.
  Member<AnimatorDefinition> definition_;
  ScopedPersistent<v8::Object> instance_;
};

}  // namespace blink

#endif  // Animator_h
