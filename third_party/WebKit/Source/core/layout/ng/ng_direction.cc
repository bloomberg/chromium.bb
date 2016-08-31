// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_direction.h"

#include "platform/text/TextDirection.h"
#include "wtf/Assertions.h"

namespace blink {

NGDirection FromPlatformDirection(TextDirection direction) {
  switch (direction) {
    case LTR:
      return LeftToRight;
    case RTL:
      return RightToLeft;
    default:
      NOTREACHED();
      return LeftToRight;
  }
}

}  // namespace blink
