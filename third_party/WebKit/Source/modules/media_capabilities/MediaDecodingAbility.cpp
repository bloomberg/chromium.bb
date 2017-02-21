// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaDecodingAbility.h"

namespace blink {

MediaDecodingAbility::MediaDecodingAbility() = default;

bool MediaDecodingAbility::supported() const {
  return true;
}

bool MediaDecodingAbility::smooth() const {
  return true;
}

bool MediaDecodingAbility::powerEfficient() const {
  return true;
}

DEFINE_TRACE(MediaDecodingAbility) {}

}  // namespace blink
