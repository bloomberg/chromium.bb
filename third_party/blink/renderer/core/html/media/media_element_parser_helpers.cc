// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace MediaElementParserHelpers {

bool ParseIntrinsicSizeAttribute(const String& value,
                                 IntSize* intrinsic_size,
                                 String* message) {
  unsigned new_width = 0, new_height = 0;
  Vector<String> size;
  value.Split('x', size);
  if (!value.IsEmpty() &&
      (size.size() != 2 ||
       !ParseHTMLNonNegativeInteger(size.at(0), new_width) ||
       !ParseHTMLNonNegativeInteger(size.at(1), new_height))) {
    *message =
        "Unable to parse intrinsicSize: expected [unsigned] x [unsigned]"
        ", got " +
        value;
    new_width = 0;
    new_height = 0;
  }
  IntSize new_size(new_width, new_height);
  if (*intrinsic_size != new_size) {
    *intrinsic_size = new_size;
    return true;
  }
  return false;
}

}  // namespace MediaElementParserHelpers

}  // namespace blink
