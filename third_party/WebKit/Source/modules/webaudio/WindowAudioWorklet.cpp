// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/WindowAudioWorklet.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/webaudio/AudioWorklet.h"

namespace blink {

WindowAudioWorklet::WindowAudioWorklet(LocalDOMWindow& window)
    : ContextLifecycleObserver(window.frame()->document()) {}

const char* WindowAudioWorklet::supplementName() {
  return "WindowAudioWorklet";
}

WindowAudioWorklet& WindowAudioWorklet::from(LocalDOMWindow& window) {
  WindowAudioWorklet* supplement = static_cast<WindowAudioWorklet*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new WindowAudioWorklet(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

Worklet* WindowAudioWorklet::audioWorklet(DOMWindow& window) {
  return from(toLocalDOMWindow(window)).audioWorklet(toLocalDOMWindow(window));
}

AudioWorklet* WindowAudioWorklet::audioWorklet(LocalDOMWindow& window) {
  if (!m_audioWorklet && getExecutionContext()) {
    DCHECK(window.frame());
    m_audioWorklet = AudioWorklet::create(window.frame());
  }
  return m_audioWorklet.get();
}

// Break the following cycle when the context gets detached.
// Otherwise, the worklet object will leak.
//
// window => window.audioWorklet
// => WindowAudioWorklet
// => AudioWorklet  <--- break this reference
// => ThreadedWorkletMessagingProxy
// => Document
// => ... => window
void WindowAudioWorklet::contextDestroyed() {
  m_audioWorklet = nullptr;
}

DEFINE_TRACE(WindowAudioWorklet) {
  visitor->trace(m_audioWorklet);
  Supplement<LocalDOMWindow>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
