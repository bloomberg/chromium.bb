// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSScale_h
#define CSSScale_h

#include "core/css/cssom/CSSTransformComponent.h"
#include "core/geometry/DOMMatrix.h"

namespace blink {

class DOMMatrix;

// Represents a scale value in a CSSTransformValue used for properties like
// "transform".
// See CSSScale.idl for more information about this class.
class CORE_EXPORT CSSScale final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSScale);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSScale* Create(double x, double y) { return new CSSScale(x, y); }
  static CSSScale* Create(double x, double y, double z) {
    return new CSSScale(x, y, z);
  }

  // Blink-internal ways of creating CSSScales.
  static CSSScale* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for attributes defined in the IDL.
  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }
  void setX(double x) { x_ = x; }
  void setY(double y) { y_ = y; }
  void setZ(double z) { z_ = z; }

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const final { return kScaleType; }
  DOMMatrix* AsMatrix() const final {
    DOMMatrix* result = DOMMatrix::Create();
    return result->scaleSelf(x_, y_, z_);
  }
  CSSFunctionValue* ToCSSValue() const final;

 private:
  CSSScale(double x, double y)
      : CSSTransformComponent(true /* is2D */), x_(x), y_(y), z_(1) {}
  CSSScale(double x, double y, double z)
      : CSSTransformComponent(false /* is2D */), x_(x), y_(y), z_(z) {}

  double x_;
  double y_;
  double z_;
};

}  // namespace blink

#endif
