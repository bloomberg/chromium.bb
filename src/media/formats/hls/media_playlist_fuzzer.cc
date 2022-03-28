// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/media_playlist.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Create a StringPiece from the given input
  const base::StringPiece source(reinterpret_cast<const char*>(data), size);

  // Try to parse it as a media playlist
  media::hls::MediaPlaylist::Parse(source,
                                   GURL("http://localhost/playlist.m3u8"));

  return 0;
}
