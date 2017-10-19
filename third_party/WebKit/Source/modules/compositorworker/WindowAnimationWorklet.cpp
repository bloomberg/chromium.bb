// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/WindowAnimationWorklet.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

namespace blink {

// static
AnimationWorklet* WindowAnimationWorklet::animationWorklet(
    LocalDOMWindow& window) {
  if (!window.GetFrame())
    return nullptr;
  return From(window).animation_worklet_.Get();
}

// Break the following cycle when the context gets detached.
// Otherwise, the worklet object will leak.
//
// window => window.animationWorklet
// => WindowAnimationWorklet
// => AnimationWorklet  <--- break this reference
// => ThreadedWorkletMessagingProxy
// => Document
// => ... => window
void WindowAnimationWorklet::ContextDestroyed(ExecutionContext*) {
  animation_worklet_ = nullptr;
}

void WindowAnimationWorklet::Trace(blink::Visitor* visitor) {
  visitor->Trace(animation_worklet_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

// static
WindowAnimationWorklet& WindowAnimationWorklet::From(LocalDOMWindow& window) {
  WindowAnimationWorklet* supplement = static_cast<WindowAnimationWorklet*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement) {
    supplement = new WindowAnimationWorklet(window);
    ProvideTo(window, SupplementName(), supplement);
  }
  return *supplement;
}

WindowAnimationWorklet::WindowAnimationWorklet(LocalDOMWindow& window)
    : ContextLifecycleObserver(window.GetFrame()->GetDocument()),
      animation_worklet_(AnimationWorklet::Create(window.GetFrame())) {
  DCHECK(GetExecutionContext());
}

const char* WindowAnimationWorklet::SupplementName() {
  return "WindowAnimationWorklet";
}

}  // namespace blink
