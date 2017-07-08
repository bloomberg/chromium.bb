// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlOverflowMenuButtonElement_h
#define MediaControlOverflowMenuButtonElement_h

#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

// Represents the overflow menu which is displayed when the width of the media
// player is small enough that at least two buttons are no longer visible.
class MediaControlOverflowMenuButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlOverflowMenuButtonElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;

 private:
  void DefaultEventHandler(Event*) override;
};

}  // namespace blink

#endif  // MediaControlOverflowMenuButtonElement_h
