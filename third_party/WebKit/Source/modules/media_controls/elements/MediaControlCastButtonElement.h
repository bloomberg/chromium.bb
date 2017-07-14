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
  bool WillRespondToMouseClickEvents() final;
  WebLocalizedString::Name GetOverflowStringName() const final;
  bool HasOverflowButton() const final;

 protected:
  const char* GetNameForHistograms() const final;

 private:
  void DefaultEventHandler(Event*) final;
  bool KeepEventInNode(Event*) final;

  bool IsPlayingRemotely() const;

  bool is_overlay_button_;
};

}  // namespace blink

#endif  // MediaControlCastButtonElement_h
