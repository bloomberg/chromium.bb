// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationTypesMap_h
#define InterpolationTypesMap_h

#include "wtf/Vector.h"
#include <memory>

namespace blink {

class InterpolationType;
class PropertyHandle;

using InterpolationTypes = Vector<std::unique_ptr<const InterpolationType>>;

class InterpolationTypesMap {
  STACK_ALLOCATED();

 public:
  virtual const InterpolationTypes& get(const PropertyHandle&) const = 0;
  virtual size_t version() const { return 0; }
};

}  // namespace blink

#endif  // InterpolationTypesMap_h
