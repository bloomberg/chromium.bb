// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/IntSize.h"

#include "platform/wtf/text/WTFString.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

IntSize::operator gfx::Size() const {
  return gfx::Size(Width(), Height());
}

std::ostream& operator<<(std::ostream& ostream, const IntSize& size) {
  return ostream << size.ToString();
}

String IntSize::ToString() const {
  return String::Format("%dx%d", Width(), Height());
}

}  // namespace blink
