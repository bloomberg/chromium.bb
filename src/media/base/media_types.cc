// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_types.h"

#include <tuple>

namespace media {

// static
AudioType AudioType::FromDecoderConfig(const AudioDecoderConfig& config) {
  return {config.codec()};
}

// static
VideoType VideoType::FromDecoderConfig(const VideoDecoderConfig& config) {
  // Level is not part of |config|. Its also not always known in the container
  // metadata (e.g. WebM puts it in CodecPrivate which is often not included).
  // Level is not used by /media to make/break support decisions, but
  // embedders with strict hardware limitations could theoretically check it.
  // The following attempts to make a safe guess by choosing the lowest level
  // for the given codec.

  // Zero is not a valid level for any of the following codecs. It means
  // "unknown" or "no level" (e.g. VP8).
  int level = 0;

  switch (config.codec()) {
    // These have no notion of level.
    case kUnknownVideoCodec:
    case kCodecTheora:
    case kCodecVP8:
    // These use non-numeric levels, aren't part of our mime code, and
    // are ancient with very limited support.
    case kCodecVC1:
    case kCodecMPEG2:
    case kCodecMPEG4:
      break;
    case kCodecH264:
    case kCodecVP9:
    case kCodecHEVC:
      // 10 is the level_idc for level 1.0.
      level = 10;
      break;
    case kCodecDolbyVision:
      // Dolby doesn't do decimals, so 1 is just 1.
      level = 1;
      break;
    case kCodecAV1:
      // Strangely, AV1 starts at 2.0.
      level = 20;
      break;
  }

  return {config.codec(), config.profile(), level, config.color_space_info()};
}

bool operator==(const AudioType& x, const AudioType& y) {
  return x.codec == y.codec;
}

bool operator!=(const AudioType& x, const AudioType& y) {
  return !(x == y);
}

bool operator==(const VideoType& x, const VideoType& y) {
  return std::tie(x.codec, x.profile, x.level, x.color_space) ==
         std::tie(y.codec, y.profile, y.level, y.color_space);
}

bool operator!=(const VideoType& x, const VideoType& y) {
  return !(x == y);
}

}  // namespace media