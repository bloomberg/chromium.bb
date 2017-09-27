// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontWidthVariant.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String ToString(FontWidthVariant variant) {
  switch (variant) {
    case FontWidthVariant::kRegularWidth:
      return "Regular";
    case FontWidthVariant::kHalfWidth:
      return "Half";
    case FontWidthVariant::kThirdWidth:
      return "Third";
    case FontWidthVariant::kQuarterWidth:
      return "Quarter";
  }
  return "Unknown";
}

}  // namespace blink
