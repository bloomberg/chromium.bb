// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMPointReadOnly.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "core/dom/DOMMatrixInit.h"
#include "core/dom/DOMMatrixReadOnly.h"
#include "core/dom/DOMPoint.h"
#include "core/dom/DOMPointInit.h"

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

DOMPointReadOnly* DOMPointReadOnly::fromPoint(const DOMPointInit& other) {
  return new DOMPointReadOnly(other.x(), other.y(), other.z(), other.w());
}

DOMPoint* DOMPointReadOnly::matrixTransform(DOMMatrixInit& other,
                                            ExceptionState& exceptionState) {
  DOMMatrixReadOnly* matrix =
      DOMMatrixReadOnly::fromMatrix(other, exceptionState);

  if (matrix->is2D() && z() == 0 && w() == 1) {
    double transformedX =
        x() * matrix->m11() + y() * matrix->m12() + matrix->m41();
    double transformedY =
        x() * matrix->m12() + y() * matrix->m22() + matrix->m42();
    return DOMPoint::create(transformedX, transformedY, 0, 1);
  }

  double transformedX = x() * matrix->m11() + y() * matrix->m21() +
                        z() * matrix->m31() + w() * matrix->m41();
  double transformedY = x() * matrix->m12() + y() * matrix->m22() +
                        z() * matrix->m32() + w() * matrix->m42();
  double transformedZ = x() * matrix->m13() + y() * matrix->m23() +
                        z() * matrix->m33() + w() * matrix->m43();
  double transformedW = x() * matrix->m14() + y() * matrix->m24() +
                        z() * matrix->m34() + w() * matrix->m44();
  return DOMPoint::create(transformedX, transformedY, transformedZ,
                          transformedW);
}

DOMPointReadOnly::DOMPointReadOnly(double x, double y, double z, double w)
    : m_x(x), m_y(y), m_z(z), m_w(w) {}

}  // namespace blink
