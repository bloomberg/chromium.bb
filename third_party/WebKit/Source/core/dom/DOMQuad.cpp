// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMQuad.h"

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/DOMPoint.h"
#include "core/dom/DOMRectInit.h"

namespace blink {

DOMQuad* DOMQuad::create(const DOMPointInit& p1,
                         const DOMPointInit& p2,
                         const DOMPointInit& p3,
                         const DOMPointInit& p4) {
  return new DOMQuad(p1, p2, p3, p4);
}

DOMQuad* DOMQuad::fromRect(const DOMRectInit& other) {
  return new DOMQuad(other.x(), other.y(), other.width(), other.height());
}

DOMQuad::DOMQuad(const DOMPointInit& p1,
                 const DOMPointInit& p2,
                 const DOMPointInit& p3,
                 const DOMPointInit& p4)
    : m_p1(DOMPoint::fromPoint(p1)),
      m_p2(DOMPoint::fromPoint(p2)),
      m_p3(DOMPoint::fromPoint(p3)),
      m_p4(DOMPoint::fromPoint(p4)) {}

DOMQuad::DOMQuad(double x, double y, double width, double height)
    : m_p1(DOMPoint::create(x, y, 0, 1)),
      m_p2(DOMPoint::create(x + width, y, 0, 1)),
      m_p3(DOMPoint::create(x + width, y + height, 0, 1)),
      m_p4(DOMPoint::create(x, y + height, 0, 1)) {}

ScriptValue DOMQuad::toJSONForBinding(ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.add("p1", p1());
  result.add("p2", p2());
  result.add("p3", p3());
  result.add("p4", p4());
  return result.scriptValue();
}

}  // namespace blink
