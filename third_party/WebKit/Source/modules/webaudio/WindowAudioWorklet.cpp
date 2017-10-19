// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/WindowAudioWorklet.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/webaudio/BaseAudioContext.h"

namespace blink {

AudioWorklet* WindowAudioWorklet::audioWorklet(LocalDOMWindow& window) {
  if (!window.GetFrame())
    return nullptr;
  return From(window).audio_worklet_.Get();
}

AudioWorklet* WindowAudioWorklet::audioWorklet(BaseAudioContext* context) {
  LocalDOMWindow* window = context->GetExecutionContext()->ExecutingWindow();
  if (!window->GetFrame())
    return nullptr;
  return From(*window).audio_worklet_.Get();
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
void WindowAudioWorklet::ContextDestroyed(ExecutionContext*) {
  audio_worklet_ = nullptr;
}

void WindowAudioWorklet::Trace(blink::Visitor* visitor) {
  visitor->Trace(audio_worklet_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

WindowAudioWorklet& WindowAudioWorklet::From(LocalDOMWindow& window) {
  WindowAudioWorklet* supplement = static_cast<WindowAudioWorklet*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement) {
    supplement = new WindowAudioWorklet(window);
    ProvideTo(window, SupplementName(), supplement);
  }
  return *supplement;
}

WindowAudioWorklet::WindowAudioWorklet(LocalDOMWindow& window)
    : ContextLifecycleObserver(window.GetFrame()->GetDocument()),
      audio_worklet_(AudioWorklet::Create(window.GetFrame())) {
  DCHECK(GetExecutionContext());
}

const char* WindowAudioWorklet::SupplementName() {
  return "WindowAudioWorklet";
}

}  // namespace blink
