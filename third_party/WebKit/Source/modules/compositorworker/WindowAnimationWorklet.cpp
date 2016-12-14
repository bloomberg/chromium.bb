// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/WindowAnimationWorklet.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/compositorworker/AnimationWorklet.h"

namespace blink {

WindowAnimationWorklet::WindowAnimationWorklet(LocalDOMWindow& window)
    : ContextLifecycleObserver(window.frame()->document()) {}

const char* WindowAnimationWorklet::supplementName() {
  return "WindowAnimationWorklet";
}

// static
WindowAnimationWorklet& WindowAnimationWorklet::from(LocalDOMWindow& window) {
  WindowAnimationWorklet* supplement = static_cast<WindowAnimationWorklet*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new WindowAnimationWorklet(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

// static
Worklet* WindowAnimationWorklet::animationWorklet(DOMWindow& window) {
  return from(toLocalDOMWindow(window))
      .animationWorklet(toLocalDOMWindow(window));
}

AnimationWorklet* WindowAnimationWorklet::animationWorklet(
    LocalDOMWindow& window) {
  if (!m_animationWorklet && getExecutionContext()) {
    DCHECK(window.frame());
    m_animationWorklet = AnimationWorklet::create(window.frame());
  }
  return m_animationWorklet.get();
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
void WindowAnimationWorklet::contextDestroyed() {
  m_animationWorklet = nullptr;
}

DEFINE_TRACE(WindowAnimationWorklet) {
  visitor->trace(m_animationWorklet);
  Supplement<LocalDOMWindow>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
