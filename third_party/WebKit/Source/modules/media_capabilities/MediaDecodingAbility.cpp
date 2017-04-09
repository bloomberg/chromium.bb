// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaDecodingAbility.h"

namespace blink {

// static
MediaDecodingAbility* MediaDecodingAbility::Take(
    ScriptPromiseResolver*,
    std::unique_ptr<WebMediaDecodingAbility> web_media_decoding_ability) {
  DCHECK(web_media_decoding_ability);
  return new MediaDecodingAbility(std::move(web_media_decoding_ability));
}

bool MediaDecodingAbility::supported() const {
  return web_media_decoding_ability_->supported;
}

bool MediaDecodingAbility::smooth() const {
  return web_media_decoding_ability_->smooth;
}

bool MediaDecodingAbility::powerEfficient() const {
  return web_media_decoding_ability_->power_efficient;
}

DEFINE_TRACE(MediaDecodingAbility) {}

MediaDecodingAbility::MediaDecodingAbility(
    std::unique_ptr<WebMediaDecodingAbility> web_media_decoding_ability)
    : web_media_decoding_ability_(std::move(web_media_decoding_ability)) {}

}  // namespace blink
