// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlButtonPanelElement_h
#define MediaControlButtonPanelElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class MediaControlsImpl;

// This element groups the buttons together in the media controls. It needs to
// be a subclass of MediaControlElementBase so it can be factored in when
// working out which elements to hide/show based on the size of the media
// element.
class MediaControlButtonPanelElement final : public MediaControlDivElement {
 public:
  explicit MediaControlButtonPanelElement(MediaControlsImpl&);
};

}  // namespace blink

#endif  // MediaControlButtonPanelElement_h
