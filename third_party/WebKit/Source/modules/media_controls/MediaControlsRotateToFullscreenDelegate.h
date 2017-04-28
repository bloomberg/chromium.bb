// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsRotateToFullscreenDelegate_h
#define MediaControlsRotateToFullscreenDelegate_h

#include "core/events/EventListener.h"
#include "modules/ModulesExport.h"

namespace blink {

class HTMLVideoElement;
class ElementVisibilityObserver;

// MediaControlsRotateToFullscreenDelegate automatically enters and exits
// fullscreen when the device is rotated whilst watching a <video>. It is meant
// to be created by `MediaControlsImpl` when the feature applies. Once created,
// it will listen for events.
class MediaControlsRotateToFullscreenDelegate final : public EventListener {
 public:
  explicit MediaControlsRotateToFullscreenDelegate(HTMLVideoElement&);

  // Called by MediaControlsImpl when the HTMLMediaElement is added to a
  // document. All event listeners should be added.
  void Attach();

  // Called by MediaControlsImpl when the HTMLMediaElement is no longer in the
  // document. All event listeners should be removed in order to prepare the
  // object to be garbage collected.
  void Detach();

  // EventListener implementation.
  bool operator==(const EventListener&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaControlsRotateToFullscreenDelegateTest;

  // Represents either screen orientation or video aspect ratio.
  enum class SimpleOrientation { kPortrait, kLandscape, kUnknown };

  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;

  void OnStateChange();
  void OnVisibilityChange(bool is_visible);
  void OnScreenOrientationChange();

  MODULES_EXPORT SimpleOrientation ComputeVideoOrientation() const;
  SimpleOrientation ComputeScreenOrientation() const;

  SimpleOrientation current_screen_orientation_ = SimpleOrientation::kUnknown;

  // Only valid when visibility_observer_ is active and the first
  // OnVisibilityChanged has been received; otherwise assume video is hidden.
  bool is_visible_ = false;

  // This is null whenever we're not listening.
  Member<ElementVisibilityObserver> visibility_observer_ = nullptr;

  // `video_element_` owns MediaControlsImpl that owns |this|.
  Member<HTMLVideoElement> video_element_;
};

}  // namespace blink

#endif  // MediaControlsRotateToFullscreenDelegate_h
