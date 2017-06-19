// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSScale_h
#define CSSScale_h

#include "core/css/cssom/CSSTransformComponent.h"
#include "core/geometry/DOMMatrix.h"

namespace blink {

class CORE_EXPORT CSSScale final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSScale);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSScale* Create(double x, double y) { return new CSSScale(x, y); }

  static CSSScale* Create(double x, double y, double z) {
    return new CSSScale(x, y, z);
  }

  static CSSScale* FromCSSValue(const CSSFunctionValue&);

  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }

  void setX(double x) { x_ = x; }
  void setY(double y) { y_ = y; }
  void setZ(double z) { z_ = z; }

  TransformComponentType GetType() const override {
    return is2d_ ? kScaleType : kScale3DType;
  }

  DOMMatrix* AsMatrix() const override {
    DOMMatrix* result = DOMMatrix::Create();
    return result->scaleSelf(x_, y_, z_);
  }

  CSSFunctionValue* ToCSSValue() const override;

 private:
  CSSScale(double x, double y) : x_(x), y_(y), z_(1), is2d_(true) {}
  CSSScale(double x, double y, double z) : x_(x), y_(y), z_(z), is2d_(false) {}

  double x_;
  double y_;
  double z_;
  bool is2d_;
};

}  // namespace blink

#endif
