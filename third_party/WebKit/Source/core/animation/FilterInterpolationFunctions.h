// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterInterpolationFunctions_h
#define FilterInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class FilterOperation;
class CSSValue;
class StyleResolverState;

namespace FilterInterpolationFunctions {

InterpolationValue MaybeConvertCSSFilter(const CSSValue&);
InterpolationValue MaybeConvertFilter(const FilterOperation&, double zoom);
std::unique_ptr<InterpolableValue> CreateNoneValue(const NonInterpolableValue&);
bool FiltersAreCompatible(const NonInterpolableValue&,
                          const NonInterpolableValue&);
FilterOperation* CreateFilter(const InterpolableValue&,
                              const NonInterpolableValue&,
                              const StyleResolverState&);

}  // namespace FilterInterpolationFunctions

}  // namespace blink

#endif  // FilterInterpolationFunctions_h
