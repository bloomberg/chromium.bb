// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowAnimationWorklet_h
#define WindowAnimationWorklet_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "modules/compositorworker/AnimationWorklet.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalDOMWindow;

class MODULES_EXPORT WindowAnimationWorklet final
    : public GarbageCollected<WindowAnimationWorklet>,
      public Supplement<LocalDOMWindow>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(WindowAnimationWorklet);

 public:
  static AnimationWorklet* animationWorklet(LocalDOMWindow&);

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*);

 private:
  static WindowAnimationWorklet& From(LocalDOMWindow&);

  explicit WindowAnimationWorklet(LocalDOMWindow&);
  static const char* SupplementName();

  Member<AnimationWorklet> animation_worklet_;
};

}  // namespace blink

#endif  // WindowAnimationWorklet_h
