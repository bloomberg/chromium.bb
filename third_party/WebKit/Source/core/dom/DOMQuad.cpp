// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMQuad.h"

#include "core/dom/DOMPoint.h"

namespace blink {

DOMQuad* DOMQuad::create(const DOMPointInit& p1,
                         const DOMPointInit& p2,
                         const DOMPointInit& p3,
                         const DOMPointInit& p4) {
  return new DOMQuad(p1, p2, p3, p4);
}

DOMQuad::DOMQuad(const DOMPointInit& p1,
                 const DOMPointInit& p2,
                 const DOMPointInit& p3,
                 const DOMPointInit& p4)
    : m_p1(DOMPoint::fromPoint(p1)),
      m_p2(DOMPoint::fromPoint(p2)),
      m_p3(DOMPoint::fromPoint(p3)),
      m_p4(DOMPoint::fromPoint(p4)) {}

}  // namespace blink
