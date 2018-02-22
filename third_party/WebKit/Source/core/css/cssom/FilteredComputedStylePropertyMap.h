// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilteredComputedStylePropertyMap_h
#define FilteredComputedStylePropertyMap_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "core/css/cssom/ComputedStylePropertyMap.h"

namespace blink {

class CORE_EXPORT FilteredComputedStylePropertyMap
    : public ComputedStylePropertyMap {
 public:
  static FilteredComputedStylePropertyMap* Create(
      Node* node,
      const Vector<CSSPropertyID>& native_properties,
      const Vector<AtomicString>& custom_properties) {
    return new FilteredComputedStylePropertyMap(node, native_properties,
                                                custom_properties);
  }

 private:
  FilteredComputedStylePropertyMap(
      Node*,
      const Vector<CSSPropertyID>& native_properties,
      const Vector<AtomicString>& custom_properties);

  const CSSValue* GetProperty(CSSPropertyID) override;
  const CSSValue* GetCustomProperty(AtomicString) override;
  void ForEachProperty(const IterationCallback&) override;

  HashSet<CSSPropertyID> native_properties_;
  HashSet<AtomicString> custom_properties_;
  DISALLOW_COPY_AND_ASSIGN(FilteredComputedStylePropertyMap);
};

}  // namespace blink

#endif  // FilteredComputedStylePropertyMap_h
