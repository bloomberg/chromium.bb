// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaStreamTrackContentHint_h
#define MediaStreamTrackContentHint_h

#include "modules/mediastream/MediaStreamTrack.h"

namespace blink {

class MediaStreamTrackContentHint final {
 public:
  static String contentHint(const MediaStreamTrack& track) {
    return track.contentHint();
  }
  static void setContentHint(MediaStreamTrack& track, const String& hint) {
    track.setContentHint(hint);
  }
};

}  // namespace blink

#endif  // MediaStreamTrack_h
