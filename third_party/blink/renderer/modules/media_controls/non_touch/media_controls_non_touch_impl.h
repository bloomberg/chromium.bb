// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_IMPL_H_

#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_media_event_listener_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MediaControlsNonTouchMediaEventListener;

class MODULES_EXPORT MediaControlsNonTouchImpl final
    : public HTMLDivElement,
      public MediaControls,
      public MediaControlsNonTouchMediaEventListenerObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsNonTouchImpl);

 public:
  static MediaControlsNonTouchImpl* Create(HTMLMediaElement&, ShadowRoot&);

  explicit MediaControlsNonTouchImpl(HTMLMediaElement&);

  // Node override.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  // MediaControls implementation.
  void MaybeShow() override;
  void Hide() override;
  void Reset() override {}
  void OnControlsListUpdated() override {}
  void OnTrackElementFailedToLoad() override {}
  void NetworkStateChanged() override {}
  LayoutObject* PanelLayoutObject() override { return nullptr; }
  LayoutObject* TimelineLayoutObject() override { return nullptr; }
  LayoutObject* ButtonPanelLayoutObject() override { return nullptr; }
  LayoutObject* ContainerLayoutObject() override { return nullptr; }
  void SetTestMode(bool) override {}
  HTMLDivElement* PanelElement() override { return nullptr; }
  void OnMediaControlsEnabledChange() override {}

  // MediaControlsNonTouchMediaEventListenerObserver implementation.
  void OnFocusIn() override;
  void OnTimeUpdate() override {}
  void OnDurationChange() override {}
  void OnPlay() override {}
  void OnPause() override {}
  void OnError() override {}
  void OnLoadedMetadata() override {}
  void OnKeyPress(KeyboardEvent* event) override {}
  void OnKeyDown(KeyboardEvent* event) override;
  void OnKeyUp(KeyboardEvent* event) override {}

  void Trace(blink::Visitor*) override;

 private:
  friend class MediaControlsNonTouchImplTest;

  enum class ArrowDirection;
  ArrowDirection OrientArrowPress(ArrowDirection direction);
  void HandleOrientedArrowPress(ArrowDirection direction);

  WebScreenOrientationType GetOrientation();

  void HandleTopButtonPress();
  void HandleBottomButtonPress();
  void HandleLeftButtonPress();
  void HandleRightButtonPress();

  void MaybeJump(int);
  void MaybeChangeVolume(double);

  // Node
  bool IsMediaControls() const override { return true; }

  Member<MediaControlsNonTouchMediaEventListener> media_event_listener_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_NON_TOUCH_MEDIA_CONTROLS_NON_TOUCH_IMPL_H_
