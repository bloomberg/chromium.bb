// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEnvironment_h
#define InterpolationEnvironment_h

#include "core/animation/InterpolationTypesMap.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class InterpolationEnvironment {
  STACK_ALLOCATED();
 public:
  virtual bool IsCSS() const { return false; }
  virtual bool IsSVG() const { return false; }

  const InterpolationTypesMap& GetInterpolationTypesMap() const {
    return interpolation_types_map_;
  }

 protected:
  virtual ~InterpolationEnvironment() = default;

  explicit InterpolationEnvironment(const InterpolationTypesMap& map)
      : interpolation_types_map_(map) {}

 private:
  const InterpolationTypesMap& interpolation_types_map_;
};

}  // namespace blink

#endif  // InterpolationEnvironment_h
