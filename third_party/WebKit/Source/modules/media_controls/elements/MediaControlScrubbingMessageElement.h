// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlScrubbingMessageElement_h
#define MediaControlScrubbingMessageElement_h

#include "modules/media_controls/elements/MediaControlDivElement.h"

namespace blink {

class MediaControlsImpl;

class MediaControlScrubbingMessageElement final
    : public MediaControlDivElement {
 public:
  explicit MediaControlScrubbingMessageElement(MediaControlsImpl&);
};

}  // namespace blink

#endif  // MediaControlScrubbingMessageElement_h
