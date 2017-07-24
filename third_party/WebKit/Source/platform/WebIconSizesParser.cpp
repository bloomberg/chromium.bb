// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebIconSizesParser.h"

#include <algorithm>
#include "platform/wtf/text/StringToNumber.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"

namespace blink {

namespace {

static inline bool IsIntegerStart(UChar c) {
  return c > '0' && c <= '9';
}

static bool IsWhitespace(UChar c) {
  // Sizes space characters are U+0020 SPACE, U+0009 CHARACTER TABULATION (tab),
  // U+000A LINE FEED (LF), U+000C FORM FEED (FF),
  // and U+000D CARRIAGE RETURN (CR).
  return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
}

static bool IsNotWhitespace(UChar c) {
  return !IsWhitespace(c);
}

static bool IsNonDigit(UChar c) {
  return !IsASCIIDigit(c);
}

static inline size_t FindEndOfWord(const String& string, size_t start) {
  return std::min(string.Find(IsWhitespace, start),
                  static_cast<size_t>(string.length()));
}

static inline int PartialStringToInt(const String& string,
                                     size_t start,
                                     size_t end) {
  if (string.Is8Bit()) {
    return CharactersToInt(string.Characters8() + start, end - start,
                           WTF::NumberParsingOptions::kNone, nullptr);
  }
  return CharactersToInt(string.Characters16() + start, end - start,
                         WTF::NumberParsingOptions::kNone, nullptr);
}

}  // namespace

WebVector<WebSize> WebIconSizesParser::ParseIconSizes(
    const WebString& web_sizes_string) {
  String sizes_string = web_sizes_string;
  Vector<WebSize> icon_sizes;
  if (sizes_string.IsEmpty())
    return icon_sizes;

  unsigned length = sizes_string.length();
  for (unsigned i = 0; i < length; ++i) {
    // Skip whitespaces.
    i = std::min(sizes_string.Find(IsNotWhitespace, i),
                 static_cast<size_t>(length));
    if (i >= length)
      break;

    // See if the current size is "any".
    if (sizes_string.FindIgnoringCase("any", i) == i &&
        (i + 3 == length || IsWhitespace(sizes_string[i + 3]))) {
      icon_sizes.push_back(WebSize(0, 0));
      i = i + 3;
      continue;
    }

    // Parse the width.
    if (!IsIntegerStart(sizes_string[i])) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    unsigned width_start = i;
    i = std::min(sizes_string.Find(IsNonDigit, i), static_cast<size_t>(length));
    if (i >= length || (sizes_string[i] != 'x' && sizes_string[i] != 'X')) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    unsigned width_end = i++;

    // Parse the height.
    if (i >= length || !IsIntegerStart(sizes_string[i])) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    unsigned height_start = i;
    i = std::min(sizes_string.Find(IsNonDigit, i), static_cast<size_t>(length));
    if (i < length && !IsWhitespace(sizes_string[i])) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    unsigned height_end = i;

    // Append the parsed size to iconSizes.
    icon_sizes.push_back(
        WebSize(PartialStringToInt(sizes_string, width_start, width_end),
                PartialStringToInt(sizes_string, height_start, height_end)));
  }
  return icon_sizes;
}

}  // namespace blink
