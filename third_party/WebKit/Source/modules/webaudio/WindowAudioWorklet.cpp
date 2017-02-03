// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/WindowAudioWorklet.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

namespace blink {

AudioWorklet* WindowAudioWorklet::audioWorklet(LocalDOMWindow& window) {
  if (!window.frame())
    return nullptr;
  return from(window).m_audioWorklet.get();
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
void WindowAudioWorklet::contextDestroyed(ExecutionContext*) {
  m_audioWorklet = nullptr;
}

DEFINE_TRACE(WindowAudioWorklet) {
  visitor->trace(m_audioWorklet);
  Supplement<LocalDOMWindow>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
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

WindowAudioWorklet::WindowAudioWorklet(LocalDOMWindow& window)
    : ContextLifecycleObserver(window.frame()->document()),
      m_audioWorklet(AudioWorklet::create(window.frame())) {
  DCHECK(getExecutionContext());
}

const char* WindowAudioWorklet::supplementName() {
  return "WindowAudioWorklet";
}

}  // namespace blink
