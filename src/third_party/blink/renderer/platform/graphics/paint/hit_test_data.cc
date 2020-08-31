// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/hit_test_data.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static String TouchActionRectsAsString(const Vector<TouchActionRect>& rects) {
  StringBuilder sb;
  sb.Append("[");
  bool first = true;
  for (const auto& rect : rects) {
    if (!first)
      sb.Append(", ");
    first = false;
    sb.Append("(");
    sb.Append(rect.ToString());
    sb.Append(")");
  }
  sb.Append("]");
  return sb.ToString();
}

String HitTestData::ToString() const {
  StringBuilder sb;
  sb.Append("{");

  bool printed_top_level_field = false;
  if (!touch_action_rects.IsEmpty()) {
    sb.Append("touch_action_rects: ");
    sb.Append(TouchActionRectsAsString(touch_action_rects));
    printed_top_level_field = true;
  }

  if (!scroll_hit_test_rect.IsEmpty()) {
    if (printed_top_level_field)
      sb.Append(", ");
    sb.Append("scroll_hit_test_rect: ");
    sb.Append(scroll_hit_test_rect.ToString());
    printed_top_level_field = true;
  }

  if (scroll_translation) {
    if (printed_top_level_field)
      sb.Append(", ");
    sb.AppendFormat("scroll_translation: %p", scroll_translation);
  }

  sb.Append("}");
  return sb.ToString();
}

std::ostream& operator<<(std::ostream& os, const HitTestData& data) {
  return os << data.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& os, const HitTestData* data) {
  return os << (data ? data->ToString().Utf8() : "null");
}

}  // namespace blink
