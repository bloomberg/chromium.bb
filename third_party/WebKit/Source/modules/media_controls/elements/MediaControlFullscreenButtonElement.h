// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlFullscreenButtonElement_h
#define MediaControlFullscreenButtonElement_h

#include "core/html/shadow/MediaControlElementTypes.h"

namespace blink {

class Event;
class MediaControlsImpl;

class MediaControlFullscreenButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlFullscreenButtonElement(MediaControlsImpl&);

  // TODO(mlamouri): this should be changed to UpdateDisplayType().
  void SetIsFullscreen(bool);

  // MediaControlInputElement overrides.
  bool WillRespondToMouseClickEvents() override;
  WebLocalizedString::Name GetOverflowStringName() override;
  bool HasOverflowButton() override;

 private:
  void DefaultEventHandler(Event*) override;
  void RecordClickMetrics();
};

}  // namespace blink

#endif  // MediaControlFullscreenButtonElement_h
