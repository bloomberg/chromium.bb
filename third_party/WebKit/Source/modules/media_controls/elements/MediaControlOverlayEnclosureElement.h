// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlOverlayEnclosureElement_h
#define MediaControlOverlayEnclosureElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class Event;
class EventDispatchHandlingState;
class MediaControlsImpl;

class MediaControlOverlayEnclosureElement final
    : public MediaControlDivElement {
 public:
  explicit MediaControlOverlayEnclosureElement(MediaControlsImpl&);

 private:
  EventDispatchHandlingState* PreDispatchEventHandler(Event*) override;
};

}  // namespace blink

#endif  // MediaControlOverlayEnclosureElement_h
