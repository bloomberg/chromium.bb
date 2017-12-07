// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MutableCSSPropertyValueSet_h
#define MutableCSSPropertyValueSet_h

#include "core/css/CSSPropertyValueSet.h"

namespace blink {

class CORE_EXPORT MutableCSSPropertyValueSet : public CSSPropertyValueSet {
 public:
  ~MutableCSSPropertyValueSet() = default;
  static MutableCSSPropertyValueSet* Create(CSSParserMode);
  static MutableCSSPropertyValueSet* Create(const CSSPropertyValue* properties,
                                            unsigned count);

  unsigned PropertyCount() const { return property_vector_.size(); }

  // Returns whether this style set was changed.
  bool AddParsedProperties(const HeapVector<CSSPropertyValue, 256>&);
  bool AddRespectingCascade(const CSSPropertyValue&);

  struct SetResult {
    bool did_parse;
    bool did_change;
  };
  // These expand shorthand properties into multiple properties.
  SetResult SetProperty(CSSPropertyID unresolved_property,
                        const String& value,
                        bool important,
                        SecureContextMode,
                        StyleSheetContents* context_style_sheet = nullptr);
  SetResult SetProperty(const AtomicString& custom_property_name,
                        const PropertyRegistry*,
                        const String& value,
                        bool important,
                        SecureContextMode,
                        StyleSheetContents* context_style_sheet,
                        bool is_animation_tainted);
  void SetProperty(CSSPropertyID, const CSSValue&, bool important = false);

  // These do not. FIXME: This is too messy, we can do better.
  bool SetProperty(CSSPropertyID,
                   CSSValueID identifier,
                   bool important = false);
  bool SetProperty(const CSSPropertyValue&, CSSPropertyValue* slot = nullptr);

  template <typename T>  // CSSPropertyID or AtomicString
  bool RemoveProperty(T property, String* return_text = nullptr);
  bool RemovePropertiesInSet(const CSSProperty** set, unsigned length);
  void RemoveEquivalentProperties(const CSSPropertyValueSet*);
  void RemoveEquivalentProperties(const CSSStyleDeclaration*);

  void MergeAndOverrideOnConflict(const CSSPropertyValueSet*);

  void Clear();
  void ParseDeclarationList(const String& style_declaration,
                            SecureContextMode,
                            StyleSheetContents* context_style_sheet);

  CSSStyleDeclaration* EnsureCSSStyleDeclaration();

  template <typename T>  // CSSPropertyID, AtRuleDescriptorID or AtomicString
  int FindPropertyIndex(T property) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  explicit MutableCSSPropertyValueSet(CSSParserMode);
  explicit MutableCSSPropertyValueSet(const CSSPropertyValueSet&);
  MutableCSSPropertyValueSet(const CSSPropertyValue* properties,
                             unsigned count);

  bool RemovePropertyAtIndex(int, String* return_text);

  bool RemoveShorthandProperty(CSSPropertyID);
  bool RemoveShorthandProperty(const AtomicString& custom_property_name) {
    return false;
  }
  CSSPropertyValue* FindCSSPropertyWithID(
      CSSPropertyID,
      const AtomicString& custom_property_name = g_null_atom);
  Member<PropertySetCSSStyleDeclaration> cssom_wrapper_;

  friend class CSSPropertyValueSet;

  HeapVector<CSSPropertyValue, 4> property_vector_;
};

DEFINE_TYPE_CASTS(MutableCSSPropertyValueSet,
                  CSSPropertyValueSet,
                  set,
                  set->IsMutable(),
                  set.IsMutable());

}  // namespace blink

#endif  // MutableCSSPropertyValueSet_h
