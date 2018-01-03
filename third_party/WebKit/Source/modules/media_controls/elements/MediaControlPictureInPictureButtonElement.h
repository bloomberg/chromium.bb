// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlPictureInPictureButtonElement_h
#define MediaControlPictureInPictureButtonElement_h

#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlPictureInPictureButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlPictureInPictureButtonElement(MediaControlsImpl&);

  // MediaControlInputElement:
  bool WillRespondToMouseClickEvents() override;
  WebLocalizedString::Name GetOverflowStringName() const override;
  bool HasOverflowButton() const override;

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(event); }

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
};

}  // namespace blink

#endif  // MediaControlPictureInPictureButtonElement_h
