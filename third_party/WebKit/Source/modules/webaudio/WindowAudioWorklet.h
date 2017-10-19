// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowAudioWorklet_h
#define WindowAudioWorklet_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "modules/webaudio/AudioWorklet.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class BaseAudioContext;
class LocalDOMWindow;

class MODULES_EXPORT WindowAudioWorklet final
    : public GarbageCollected<WindowAudioWorklet>,
      public Supplement<LocalDOMWindow>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(WindowAudioWorklet);

 public:
  static AudioWorklet* audioWorklet(LocalDOMWindow&);
  static AudioWorklet* audioWorklet(BaseAudioContext*);

  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*);

 private:
  static WindowAudioWorklet& From(LocalDOMWindow&);

  explicit WindowAudioWorklet(LocalDOMWindow&);
  static const char* SupplementName();

  Member<AudioWorklet> audio_worklet_;
};

}  // namespace blink

#endif  // WindowAudioWorklet_h
