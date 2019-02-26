// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_metadata.h"

#include <algorithm>
#include <iterator>

namespace media_session {

MediaMetadata::MediaImage::MediaImage() = default;

MediaMetadata::MediaImage::MediaImage(const MediaImage& other) = default;

MediaMetadata::MediaImage::~MediaImage() = default;

bool MediaMetadata::MediaImage::operator==(
    const MediaMetadata::MediaImage& other) const {
  return src == other.src && type == other.type && sizes == other.sizes;
}

MediaMetadata::MediaMetadata() = default;

MediaMetadata::~MediaMetadata() = default;

MediaMetadata::MediaMetadata(const MediaMetadata& other) = default;

bool MediaMetadata::operator==(const MediaMetadata& other) const {
  return title == other.title && artist == other.artist &&
         album == other.album && artwork == other.artwork;
}

bool MediaMetadata::operator!=(const MediaMetadata& other) const {
  return !(*this == other);
}

}  // namespace media_session
