// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

TextFragmentSelector TextFragmentSelector::Create(const String& target_text) {
  SelectorType type;
  String start;
  String end;

  size_t comma_pos = target_text.find(',');

  if (comma_pos == kNotFound) {
    type = kExact;
    start = target_text;
    end = "";
  } else {
    type = kRange;
    start = target_text.Substring(0, comma_pos);
    end = target_text.Substring(comma_pos + 1);
  }

  return TextFragmentSelector(
      type, DecodeURLEscapeSequences(start, DecodeURLMode::kUTF8),
      DecodeURLEscapeSequences(end, DecodeURLMode::kUTF8));
}

TextFragmentSelector::TextFragmentSelector(SelectorType type,
                                           String start,
                                           String end)
    : type_(type), start_(start), end_(end) {}

}  // namespace blink
