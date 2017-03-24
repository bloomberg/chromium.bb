// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMRect.h"

#include "core/dom/DOMRectInit.h"

namespace blink {

DOMRect* DOMRect::create(double x, double y, double width, double height) {
  return new DOMRect(x, y, width, height);
}

DOMRect* DOMRect::fromRect(const DOMRectInit& other) {
  return new DOMRect(other.x(), other.y(), other.width(), other.height());
}

DOMRect::DOMRect(double x, double y, double width, double height)
    : DOMRectReadOnly(x, y, width, height) {}

}  // namespace blink
