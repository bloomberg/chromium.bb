// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMPointReadOnly.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"

namespace blink {

DOMPointReadOnly* DOMPointReadOnly::create(double x,
                                           double y,
                                           double z,
                                           double w) {
  return new DOMPointReadOnly(x, y, z, w);
}

ScriptValue DOMPointReadOnly::toJSONForBinding(
    ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.addNumber("x", x());
  result.addNumber("y", y());
  result.addNumber("z", z());
  result.addNumber("w", w());
  return result.scriptValue();
}

DOMPointReadOnly::DOMPointReadOnly(double x, double y, double z, double w)
    : m_x(x), m_y(y), m_z(z), m_w(w) {}

}  // namespace blink
