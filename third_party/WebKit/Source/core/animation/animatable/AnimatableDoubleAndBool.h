// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimatableDoubleAndBool_h
#define AnimatableDoubleAndBool_h

#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"

namespace blink {

class AnimatableDoubleAndBool final : public AnimatableValue {
 public:
  ~AnimatableDoubleAndBool() override {}
  static PassRefPtr<AnimatableDoubleAndBool> Create(double number, bool flag) {
    return AdoptRef(new AnimatableDoubleAndBool(number, flag));
  }

 private:
  AnimatableDoubleAndBool(double number, bool flag)
      : number_(number), flag_(flag) {}
  AnimatableType GetType() const override { return kTypeDoubleAndBool; }
  bool EqualTo(const AnimatableValue*) const override;

  double number_;
  bool flag_;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableDoubleAndBool, IsDoubleAndBool());

}  // namespace blink

#endif  // AnimatableDoubleAndBool_h
