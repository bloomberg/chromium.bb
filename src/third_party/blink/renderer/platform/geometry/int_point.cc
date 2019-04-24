// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/int_point.h"

#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const IntPoint& point) {
  return ostream << point.ToString();
}

IntPoint::operator gfx::Point() const {
  return gfx::Point(X(), Y());
}

IntPoint::operator gfx::Vector2d() const {
  return gfx::Vector2d(X(), Y());
}

String IntPoint::ToString() const {
  return String::Format("%d,%d", X(), Y());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const IntPoint& p) {
  return ts << "(" << p.X() << "," << p.Y() << ")";
}

}  // namespace blink
