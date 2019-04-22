// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_MEDIA_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_MEDIA_EVENT_LISTENER_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"

namespace blink {

class HTMLMediaElement;
class MediaControlsTouchlessMediaEventListenerObserver;

class MediaControlsTouchlessMediaEventListener final
    : public NativeEventListener {
 public:
  explicit MediaControlsTouchlessMediaEventListener(HTMLMediaElement&);

  void AddObserver(MediaControlsTouchlessMediaEventListenerObserver*);
  void RemoveObserver(MediaControlsTouchlessMediaEventListenerObserver*);

  void Attach();
  void Detach();

  void Trace(blink::Visitor*) override;
  void Invoke(ExecutionContext*, Event*) override;

 private:
  HTMLMediaElement& GetMediaElement();

  HeapListHashSet<Member<MediaControlsTouchlessMediaEventListenerObserver>>
      observers_;
  Member<HTMLMediaElement> media_element_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsTouchlessMediaEventListener);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_MEDIA_EVENT_LISTENER_H_
