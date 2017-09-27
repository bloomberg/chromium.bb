// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontOrientation.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String ToString(FontOrientation orientation) {
  switch (orientation) {
    case FontOrientation::kHorizontal:
      return "Horizontal";
    case FontOrientation::kVerticalRotated:
      return "VerticalRotated";
    case FontOrientation::kVerticalMixed:
      return "VerticalMixed";
    case FontOrientation::kVerticalUpright:
      return "VerticalUpright";
  }
  return "Unknown";
}

}  // namespace blink
