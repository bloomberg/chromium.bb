// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlCastButtonElement_h
#define MediaControlCastButtonElement_h

#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlCastButtonElement final : public MediaControlInputElement {
 public:
  MediaControlCastButtonElement(MediaControlsImpl&, bool is_overlay_button);

  // This will show a cast button if it is not covered by another element.
  // This MUST be called for cast button elements that are overlay elements.
  void TryShowOverlay();

  // TODO(avayvod): replace with the button listening to the state change
  // events.
  void UpdateDisplayType();

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;
  WebLocalizedString::Name GetOverflowStringName() const override;
  bool HasOverflowButton() const override;

 private:
  // This is used for UMA histogram (Cast.Sender.Overlay). New values should
  // be appended only and must be added before |Count|.
  enum class CastOverlayMetrics {
    kCreated = 0,
    kShown,
    kClicked,
    kCount  // Keep last.
  };

  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  void RecordMetrics(CastOverlayMetrics);

  bool IsPlayingRemotely() const;

  bool is_overlay_button_;

  // UMA related boolean. They are used to prevent counting something twice
  // for the same media element.
  bool click_use_counted_ = false;
  bool show_use_counted_ = false;
};

}  // namespace blink

#endif  // MediaControlCastButtonElement_h
