// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct SameSizeAsPaintChunk {
  size_t begin;
  size_t end;
  PaintChunk::Id id;
  PropertyTreeState properties;
  unsigned bools;
  float extend;
  FloatRect bounds;
  void* pointers[1];
};

static_assert(sizeof(PaintChunk) == sizeof(SameSizeAsPaintChunk),
              "PaintChunk should stay small");

String PaintChunk::ToString() const {
  StringBuilder sb;
  sb.Append("PaintChunk(");
  sb.Append(String::Format(
      "begin=%zu, end=%zu, id=%s cacheable=%d props=(%s) bounds=%s "
      "known_to_be_opaque=%d",
      begin_index, end_index, id.ToString().Utf8().data(), is_cacheable,
      properties.ToString().Utf8().data(), bounds.ToString().Utf8().data(),
      known_to_be_opaque));
  if (hit_test_data) {
    sb.Append(", hit_test_data=");
    sb.Append(hit_test_data->ToString());
  }
  sb.Append(")");
  return sb.ToString();
}

std::ostream& operator<<(std::ostream& os, const PaintChunk& chunk) {
  return os << chunk.ToString().Utf8().data() << "\n";
}

}  // namespace blink
