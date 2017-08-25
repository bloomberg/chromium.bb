// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/GeometryPrinters.h"

#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatBox.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/text/WTFString.h"
#include <ostream>  // NOLINT

namespace blink {

void PrintTo(const DoublePoint& point, std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const DoubleRect& rect, std::ostream* os) {
  *os << rect.ToString();
}

void PrintTo(const DoubleSize& size, std::ostream* os) {
  *os << size.ToString();
}

void PrintTo(const FloatBox& box, std::ostream* os) {
  *os << box.ToString();
}

void PrintTo(const FloatPoint& point, std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const FloatPoint3D& point, std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const FloatQuad& quad, std::ostream* os) {
  *os << quad.ToString();
}

void PrintTo(const FloatRect& rect, std::ostream* os) {
  *os << rect.ToString();
}

void PrintTo(const FloatRoundedRect& rounded_rect, std::ostream* os) {
  *os << rounded_rect.ToString();
}

void PrintTo(const FloatRoundedRect::Radii& radii, std::ostream* os) {
  *os << radii.ToString();
}

void PrintTo(const FloatSize& size, std::ostream* os) {
  *os << size.ToString();
}

void PrintTo(const IntPoint& point, std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const IntRect& rect, std::ostream* os) {
  *os << rect.ToString();
}

void PrintTo(const IntSize& size, std::ostream* os) {
  *os << size.ToString();
}

void PrintTo(const LayoutPoint& point, std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const LayoutRect& rect, std::ostream* os) {
  *os << rect.ToString();
}

void PrintTo(const LayoutSize& size, std::ostream* os) {
  *os << size.ToString();
}

}  // namespace blink
