// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationTypesMap_h
#define CSSInterpolationTypesMap_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/InterpolationTypesMap.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSSyntaxDescriptor;
class PropertyRegistry;

using CSSInterpolationTypes = Vector<std::unique_ptr<CSSInterpolationType>>;

class CSSInterpolationTypesMap : public InterpolationTypesMap {
 public:
  CSSInterpolationTypesMap(const PropertyRegistry* registry)
      : registry_(registry) {}

  const InterpolationTypes& Get(const PropertyHandle&) const final;
  size_t Version() const final;

  static CSSInterpolationTypes CreateCSSInterpolationTypesForSyntax(
      const AtomicString& property_name,
      const CSSSyntaxDescriptor&);

 private:
  Member<const PropertyRegistry> registry_;
};

}  // namespace blink

#endif  // CSSInterpolationTypesMap_h
