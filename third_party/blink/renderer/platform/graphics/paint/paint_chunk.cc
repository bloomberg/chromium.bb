// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String PaintChunk::ToString() const {
  return String::Format(
      "PaintChunk(begin=%zu, end=%zu, id=%s cacheable=%d props=(%s) bounds=%s "
      "known_to_be_opaque=%d raster_invalidation_rects=%zu)",
      begin_index, end_index, id.ToString().Ascii().data(), is_cacheable,
      properties.ToString().Ascii().data(), bounds.ToString().Ascii().data(),
      known_to_be_opaque, raster_invalidation_rects.size());
}

std::ostream& operator<<(std::ostream& os, const PaintChunk& chunk) {
  return os << chunk.ToString().Utf8().data();
}

}  // namespace blink
