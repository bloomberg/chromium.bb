// Copyright 2017 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeclaredStylePropertyMap_h
#define DeclaredStylePropertyMap_h

#include "core/css/cssom/StylePropertyMap.h"

namespace blink {

class CSSStyleRule;
class StyleRule;

// This class implements declared StylePropertMap in the Typed CSSOM
// API. The specification is here:
// https://drafts.css-houdini.org/css-typed-om-1/#declared-stylepropertymap-objects
//
// The declared StylePropertyMap retrieves styles specified by a CSS style rule
// and returns them as CSSStyleValues. The IDL for this class is in
// StylePropertyMap.idl. The declared StylePropertyMap for an element is
// accessed via CSSStyleRule.attributeStyleMap (see CSSStyleRule.idl)
class CORE_EXPORT DeclaredStylePropertyMap final : public StylePropertyMap {
  WTF_MAKE_NONCOPYABLE(DeclaredStylePropertyMap);

 public:
  explicit DeclaredStylePropertyMap(CSSStyleRule* owner_rule);

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(owner_rule_);
    StylePropertyMap::Trace(visitor);
  }

 protected:
  const CSSValue* GetProperty(CSSPropertyID) override;
  const CSSValue* GetCustomProperty(AtomicString) override;
  void ForEachProperty(const IterationCallback&) override;
  void SetProperty(CSSPropertyID, const CSSValue&) override;
  void SetCustomProperty(const AtomicString&, const CSSValue&) override;
  void RemoveProperty(CSSPropertyID) override;
  void RemoveCustomProperty(const AtomicString&) override;

 private:
  StyleRule* GetStyleRule() const;

  WeakMember<CSSStyleRule> owner_rule_;
};

}  // namespace blink

#endif
