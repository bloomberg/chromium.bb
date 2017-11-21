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
      CSSComputedStyleDeclaration* computed_style_declaration,
      const Vector<CSSPropertyID>& native_properties,
      const Vector<AtomicString>& custom_properties,
      Node* node) {
    return new FilteredComputedStylePropertyMap(
        computed_style_declaration, native_properties, custom_properties, node);
  }

  CSSStyleValue* get(const String& property_name, ExceptionState&) override;
  CSSStyleValueVector getAll(const String& property_name,
                             ExceptionState&) override;
  bool has(const String& property_name, ExceptionState&) override;

  Vector<String> getProperties() override;

 private:
  FilteredComputedStylePropertyMap(
      CSSComputedStyleDeclaration*,
      const Vector<CSSPropertyID>& native_properties,
      const Vector<AtomicString>& custom_properties,
      Node*);

  HashSet<CSSPropertyID> native_properties_;
  HashSet<AtomicString> custom_properties_;
  DISALLOW_COPY_AND_ASSIGN(FilteredComputedStylePropertyMap);
};

}  // namespace blink

#endif  // FilteredComputedStylePropertyMap_h
