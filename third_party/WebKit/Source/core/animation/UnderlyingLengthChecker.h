// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingLengthChecker_h
#define UnderlyingLengthChecker_h

#include <memory>
#include "core/animation/InterpolableValue.h"
#include "core/animation/InterpolationType.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class UnderlyingLengthChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<UnderlyingLengthChecker> Create(
      size_t underlying_length) {
    return WTF::WrapUnique(new UnderlyingLengthChecker(underlying_length));
  }

  static size_t GetUnderlyingLength(const InterpolationValue& underlying) {
    if (!underlying)
      return 0;
    return ToInterpolableList(*underlying.interpolable_value).length();
  }

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return underlying_length_ == GetUnderlyingLength(underlying);
  }

 private:
  UnderlyingLengthChecker(size_t underlying_length)
      : underlying_length_(underlying_length) {}

  size_t underlying_length_;
};

}  // namespace blink

#endif  // UnderlyingLengthChecker_h
