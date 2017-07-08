// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlPanelEnclosureElement_h
#define MediaControlPanelEnclosureElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class MediaControlsImpl;

class MediaControlPanelEnclosureElement final : public MediaControlDivElement {
 public:
  explicit MediaControlPanelEnclosureElement(MediaControlsImpl&);
};

}  // namespace blink

#endif  // MediaControlPanelEnclosureElement_h
