// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlVolumeSliderElement_h
#define MediaControlVolumeSliderElement_h

#include "modules/media_controls/elements/MediaControlSliderElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlVolumeSliderElement final : public MediaControlSliderElement {
 public:
  explicit MediaControlVolumeSliderElement(MediaControlsImpl&);

  // TODO: who calls this?
  void SetVolume(double);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseMoveEvents() override;
  bool WillRespondToMouseClickEvents() override;

  void OnMediaKeyboardEvent(Event* event) { DefaultEventHandler(event); }

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;
  void SetVolumeInternal(double);
};

}  // namespace blink

#endif  // MediaControlVolumeSliderElement_h
