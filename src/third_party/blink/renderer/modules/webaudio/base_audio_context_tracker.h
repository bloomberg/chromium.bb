// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BASE_AUDIO_CONTEXT_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BASE_AUDIO_CONTEXT_TRACKER_H_

#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class BaseAudioContext;
class Document;
class InspectorWebAudioAgent;
class Page;

class MODULES_EXPORT BaseAudioContextTracker final
    : public GarbageCollected<BaseAudioContextTracker>,
      public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(BaseAudioContextTracker);

 public:
  static const char kSupplementName[];

  static void ProvideToPage(Page&);

  BaseAudioContextTracker();

  void Trace(blink::Visitor*) override;

  void SetInspectorAgent(InspectorWebAudioAgent*);

  // Notify an associated inspector agent when a BaseAudioContext is created.
  void DidCreateBaseAudioContext(BaseAudioContext*);

  // Notify an associated inspector agent when a BaseAudioContext is destroyed.
  void DidDestroyBaseAudioContext(BaseAudioContext*);

  // Notify an associated inspector agent when a BaseAudioContext is changed.
  void DidChangeBaseAudioContext(BaseAudioContext*);

  BaseAudioContext* GetContextById(const String contextId);

  static BaseAudioContextTracker* FromPage(Page*);
  static BaseAudioContextTracker* FromDocument(const Document&);

 private:
  Member<InspectorWebAudioAgent> inspector_agent_;
  HeapHashSet<WeakMember<BaseAudioContext>> contexts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_BASE_AUDIO_CONTEXT_TRACKER_H_
