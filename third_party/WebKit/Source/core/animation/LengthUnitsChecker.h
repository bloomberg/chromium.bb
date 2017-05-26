// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthUnitsChecker_h
#define LengthUnitsChecker_h

#include <memory>
#include "core/animation/CSSInterpolationType.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class LengthUnitsChecker : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<LengthUnitsChecker> MaybeCreate(
      CSSLengthArray&& length_array,
      const StyleResolverState& state) {
    bool create = false;
    size_t last_index = 0;
    for (size_t i = 0; i < length_array.values.size(); i++) {
      if (i == CSSPrimitiveValue::kUnitTypePercentage ||
          !length_array.type_flags.Get(i))
        continue;
      length_array.values[i] = LengthUnit(i, state.CssToLengthConversionData());
      create = true;
      last_index = i;
    }
    if (!create)
      return nullptr;
    return WTF::WrapUnique(
        new LengthUnitsChecker(std::move(length_array), last_index));
  }

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    for (size_t i = 0; i <= last_index_; i++) {
      if (i == CSSPrimitiveValue::kUnitTypePercentage ||
          !length_array_.type_flags.Get(i))
        continue;
      if (length_array_.values[i] !=
          LengthUnit(i, state.CssToLengthConversionData()))
        return false;
    }
    return true;
  }

  static double LengthUnit(size_t length_unit_type,
                           const CSSToLengthConversionData& conversion_data) {
    return conversion_data.ZoomedComputedPixels(
        1,
        CSSPrimitiveValue::LengthUnitTypeToUnitType(
            static_cast<CSSPrimitiveValue::LengthUnitType>(length_unit_type)));
  }

 private:
  LengthUnitsChecker(CSSPrimitiveValue::CSSLengthArray&& length_array,
                     size_t last_index)
      : length_array_(std::move(length_array)), last_index_(last_index) {}

  const CSSLengthArray length_array_;
  const size_t last_index_;
};

}  // namespace blink

#endif  // LengthUnitsChecker_h
