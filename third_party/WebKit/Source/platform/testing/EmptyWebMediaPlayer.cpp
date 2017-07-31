// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/EmptyWebMediaPlayer.h"

#include "public/platform/WebSize.h"
#include "public/platform/WebTimeRange.h"

namespace blink {

WebTimeRanges EmptyWebMediaPlayer::Buffered() const {
  return WebTimeRanges();
}

WebTimeRanges EmptyWebMediaPlayer::Seekable() const {
  return WebTimeRanges();
}

WebSize EmptyWebMediaPlayer::NaturalSize() const {
  return WebSize(0, 0);
}

WebSize EmptyWebMediaPlayer::VisibleRect() const {
  return WebSize();
}

WebString EmptyWebMediaPlayer::GetErrorMessage() const {
  return WebString();
}

}  // namespace blink
