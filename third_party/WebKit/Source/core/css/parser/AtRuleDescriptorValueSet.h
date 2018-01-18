// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AtRuleDescriptorValueSet_h
#define AtRuleDescriptorValueSet_h

#include "core/css/AtRuleDescriptorSerializer.h"
#include "core/css/CSSPropertyValue.h"
#include "core/css/parser/AtRuleDescriptors.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/dom/ExecutionContext.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

class CORE_EXPORT AtRuleDescriptorValueSet
    : public GarbageCollectedFinalized<AtRuleDescriptorValueSet> {
 public:
  enum AtRuleType {
    kFontFaceType,
  };

  static AtRuleDescriptorValueSet* Create(
      const HeapVector<blink::CSSPropertyValue, 256>& properties,
      CSSParserMode,
      AtRuleType);

  String AsText() const;
  bool HasFailedOrCanceledSubresources() const;
  const CSSValue* GetPropertyCSSValue(AtRuleDescriptorID) const;
  void SetProperty(AtRuleDescriptorID, const String& value, SecureContextMode);
  void SetProperty(AtRuleDescriptorID, const CSSValue&);
  void RemoveProperty(AtRuleDescriptorID);

  unsigned Length() const { return values_.size(); }
  String DescriptorAt(unsigned index) const;

  AtRuleDescriptorValueSet* MutableCopy() const;
  bool IsMutable() const { return true; }

  void ParseDeclarationList(const String& declaration, SecureContextMode);

  AtRuleType GetType() const { return type_; }

  void Trace(blink::Visitor*);

 private:
  AtRuleDescriptorValueSet(CSSParserMode mode, AtRuleType type)
      : mode_(mode), type_(type) {}

  bool Contains(AtRuleDescriptorID) const;
  int IndexOf(AtRuleDescriptorID) const;

  CSSParserMode mode_;
  AtRuleType type_;
  Vector<std::pair<AtRuleDescriptorID, const CSSValue*>, numAtRuleDescriptors>
      values_;
};

}  // namespace blink

#endif  // AtRuleDescriptorValueSet_h
