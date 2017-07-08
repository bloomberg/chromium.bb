// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlOverflowMenuListElement_h
#define MediaControlOverflowMenuListElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class Event;
class MediaControlsImpl;

// Holds a list of elements within the overflow menu.
class MediaControlOverflowMenuListElement final
    : public MediaControlDivElement {
 public:
  explicit MediaControlOverflowMenuListElement(MediaControlsImpl&);

 private:
  void DefaultEventHandler(Event*) override;
};

}  // namespace blink

#endif  // MediaControlOverflowMenuListElement_h
