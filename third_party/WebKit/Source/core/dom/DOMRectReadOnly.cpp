// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMRectReadOnly.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/DOMRectInit.h"

namespace blink {

DOMRectReadOnly* DOMRectReadOnly::create(double x,
                                         double y,
                                         double width,
                                         double height) {
  return new DOMRectReadOnly(x, y, width, height);
}

ScriptValue DOMRectReadOnly::toJSONForBinding(ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.addNumber("x", x());
  result.addNumber("y", y());
  result.addNumber("width", width());
  result.addNumber("height", height());
  result.addNumber("top", top());
  result.addNumber("right", right());
  result.addNumber("bottom", bottom());
  result.addNumber("left", left());
  return result.scriptValue();
}

DOMRectReadOnly* DOMRectReadOnly::fromRect(const DOMRectInit& other) {
  return new DOMRectReadOnly(other.x(), other.y(), other.width(), other.height());
}

DOMRectReadOnly::DOMRectReadOnly(double x,
                                 double y,
                                 double width,
                                 double height)
    : m_x(x), m_y(y), m_width(width), m_height(height) {}

}  // namespace blink
