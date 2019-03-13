// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_MEDIA_EVENT_LISTENER_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_MEDIA_EVENT_LISTENER_OBSERVER_H_

namespace blink {

class KeyboardEvent;

class MediaControlsNonTouchMediaEventListenerObserver
    : public GarbageCollectedMixin {
 public:
  virtual void OnFocusIn() = 0;
  virtual void OnTimeUpdate() = 0;
  virtual void OnDurationChange() = 0;
  virtual void OnPlay() = 0;
  virtual void OnPause() = 0;
  virtual void OnError() = 0;
  virtual void OnLoadedMetadata() = 0;
  virtual void OnKeyPress(KeyboardEvent* event) = 0;
  virtual void OnKeyDown(KeyboardEvent* event) = 0;
  virtual void OnKeyUp(KeyboardEvent* event) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_MEDIA_EVENT_LISTENER_OBSERVER_H_
