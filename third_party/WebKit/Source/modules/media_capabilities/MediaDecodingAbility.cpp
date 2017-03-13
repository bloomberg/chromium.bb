// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaDecodingAbility.h"

namespace blink {

// static
MediaDecodingAbility* MediaDecodingAbility::take(
    ScriptPromiseResolver*,
    std::unique_ptr<WebMediaDecodingAbility> webMediaDecodingAbility) {
  DCHECK(webMediaDecodingAbility);
  return new MediaDecodingAbility(std::move(webMediaDecodingAbility));
}

bool MediaDecodingAbility::supported() const {
  return m_webMediaDecodingAbility->supported;
}

bool MediaDecodingAbility::smooth() const {
  return m_webMediaDecodingAbility->smooth;
}

bool MediaDecodingAbility::powerEfficient() const {
  return m_webMediaDecodingAbility->powerEfficient;
}

DEFINE_TRACE(MediaDecodingAbility) {}

MediaDecodingAbility::MediaDecodingAbility(
    std::unique_ptr<WebMediaDecodingAbility> webMediaDecodingAbility)
    : m_webMediaDecodingAbility(std::move(webMediaDecodingAbility)) {}

}  // namespace blink
