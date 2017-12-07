// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlOverlayPlayButtonElement_h
#define MediaControlOverlayPlayButtonElement_h

#include "modules/media_controls/elements/MediaControlInputElement.h"

namespace blink {

class Event;
class HTMLDivElement;
class MediaControlsImpl;

class MediaControlOverlayPlayButtonElement final
    : public MediaControlInputElement {
 public:
  explicit MediaControlOverlayPlayButtonElement(MediaControlsImpl&);

  // MediaControlInputElement overrides.
  void UpdateDisplayType() override;

  WebSize GetSizeOrDefault() const final;

  void Trace(blink::Visitor*) override;

 protected:
  const char* GetNameForHistograms() const override;

 private:
  void DefaultEventHandler(Event*) override;
  bool KeepEventInNode(Event*) override;

  Member<HTMLDivElement> internal_button_;
};

}  // namespace blink

#endif  // MediaControlOverlayPlayButtonElement_h
