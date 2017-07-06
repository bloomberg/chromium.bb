// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlCurrentTimeDisplayElement_h
#define MediaControlCurrentTimeDisplayElement_h

#include "modules/media_controls/elements/MediaControlTimeDisplayElement.h"

namespace blink {

class MediaControlsImpl;

class MediaControlCurrentTimeDisplayElement final
    : public MediaControlTimeDisplayElement {
 public:
  explicit MediaControlCurrentTimeDisplayElement(MediaControlsImpl&);
};

}  // namespace blink

#endif  // MediaControlCurrentTimeDisplayElement_h
