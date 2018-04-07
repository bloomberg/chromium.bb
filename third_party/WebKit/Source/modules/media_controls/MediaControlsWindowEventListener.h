// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_WINDOW_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_WINDOW_EVENT_LISTENER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"

namespace blink {

class Element;
class MediaControlsImpl;

class MediaControlsWindowEventListener final : public EventListener {
 public:
  using Callback = base::RepeatingCallback<void()>;

  static MediaControlsWindowEventListener* Create(MediaControlsImpl*, Callback);

  bool operator==(const EventListener&) const override;

  void Start();
  void Stop();

  virtual void Trace(blink::Visitor*);

 private:
  explicit MediaControlsWindowEventListener(MediaControlsImpl*, Callback);

  void handleEvent(ExecutionContext*, Event*) override;

  // Adds or removes a click event listener if the provided element is not null.
  void MaybeAddClickEventListener(Element*);
  void MaybeRemoveClickEventListener(Element*);

  Member<MediaControlsImpl> media_controls_;
  Callback callback_;
  bool is_active_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_WINDOW_EVENT_LISTENER_H_
