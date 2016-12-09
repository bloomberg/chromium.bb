// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInterpolationTypesMap_h
#define SVGInterpolationTypesMap_h

#include "core/animation/InterpolationTypesMap.h"

namespace blink {

class SVGInterpolationTypesMap : public InterpolationTypesMap {
 public:
  SVGInterpolationTypesMap() {}

  const InterpolationTypes& get(const PropertyHandle&) const final;
};

}  // namespace blink

#endif  // SVGInterpolationTypesMap_h
