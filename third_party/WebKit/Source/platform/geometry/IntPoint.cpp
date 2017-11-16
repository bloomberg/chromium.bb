// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/IntPoint.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const IntPoint& point) {
  return ostream << point.ToString();
}

String IntPoint::ToString() const {
  return String::Format("%d,%d", X(), Y());
}

}  // namespace blink
