// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationTypesMap_h
#define CSSInterpolationTypesMap_h

#include "core/CoreExport.h"
#include "core/animation/CSSInterpolationType.h"
#include "core/animation/InterpolationTypesMap.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSSyntaxDescriptor;
class PropertyRegistry;

class CORE_EXPORT CSSInterpolationTypesMap : public InterpolationTypesMap {
 public:
  CSSInterpolationTypesMap(const PropertyRegistry* registry)
      : registry_(registry) {}

  const InterpolationTypes& Get(const PropertyHandle&) const final;
  size_t Version() const final;

  static InterpolationTypes CreateInterpolationTypesForCSSSyntax(
      const AtomicString& property_name,
      const CSSSyntaxDescriptor&,
      const PropertyRegistration&);

 private:
  Member<const PropertyRegistry> registry_;
};

}  // namespace blink

#endif  // CSSInterpolationTypesMap_h
