// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlRemainingTimeDisplayElement_h
#define MediaControlRemainingTimeDisplayElement_h

#include "modules/media_controls/elements/MediaControlTimeDisplayElement.h"

namespace blink {

class MediaControlsImpl;

class MediaControlRemainingTimeDisplayElement final
    : public MediaControlTimeDisplayElement {
 public:
  explicit MediaControlRemainingTimeDisplayElement(MediaControlsImpl&);
};

}  // namespace blink

#endif  // MediaControlRemainingTimeDisplayElement_h
