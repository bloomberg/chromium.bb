// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/EmptyWebMediaPlayer.h"

#include "public/platform/WebSize.h"
#include "public/platform/WebTimeRange.h"

namespace blink {

WebTimeRanges EmptyWebMediaPlayer::buffered() const {
  return WebTimeRanges();
}

WebTimeRanges EmptyWebMediaPlayer::seekable() const {
  return WebTimeRanges();
}

WebSize EmptyWebMediaPlayer::naturalSize() const {
  return WebSize(0, 0);
}

}  // namespace blink
