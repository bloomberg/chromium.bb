// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_MEDIA_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_MEDIA_EVENT_LISTENER_H_

#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"

namespace blink {

class HTMLMediaElement;
class MediaControlsNonTouchMediaEventListenerObserver;

class MediaControlsNonTouchMediaEventListener final
    : public NativeEventListener {
 public:
  explicit MediaControlsNonTouchMediaEventListener(HTMLMediaElement&);

  void AddObserver(MediaControlsNonTouchMediaEventListenerObserver*);
  void RemoveObserver(MediaControlsNonTouchMediaEventListenerObserver*);

  void Attach();
  void Detach();

  void Trace(blink::Visitor*) override;
  void Invoke(ExecutionContext*, Event*) override;

 private:
  HTMLMediaElement& GetMediaElement();

  HeapListHashSet<Member<MediaControlsNonTouchMediaEventListenerObserver>>
      observers_;
  Member<HTMLMediaElement> media_element_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsNonTouchMediaEventListener);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_MEDIA_EVENT_LISTENER_H_
