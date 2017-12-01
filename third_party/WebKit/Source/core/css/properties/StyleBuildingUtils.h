// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleBuildingUtils_h
#define StyleBuildingUtils_h

#include "core/style/BorderImageLength.h"
#include "core/style/BorderImageLengthBox.h"
#include "platform/Length.h"
#include "platform/LengthBox.h"

namespace blink {
namespace StyleBuildingUtils {

inline bool borderImageLengthMatchesAllSides(
    const BorderImageLengthBox& borderImageLengthBox,
    const BorderImageLength& borderImageLength) {
  return (borderImageLengthBox.Left() == borderImageLength &&
          borderImageLengthBox.Right() == borderImageLength &&
          borderImageLengthBox.Top() == borderImageLength &&
          borderImageLengthBox.Bottom() == borderImageLength);
}
inline bool lengthMatchesAllSides(const LengthBox& lengthBox,
                                  const Length& length) {
  return (lengthBox.Left() == length && lengthBox.Right() == length &&
          lengthBox.Top() == length && lengthBox.Bottom() == length);
}

}  // namespace StyleBuildingUtils
}  // namespace blink

#endif  // StyleBuildingUtils_h
