// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCustomPropertyDeclaration_h
#define CSSCustomPropertyDeclaration_h

#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CSSCustomPropertyDeclaration : public CSSValue {
 public:
  static CSSCustomPropertyDeclaration* Create(
      const AtomicString& name,
      scoped_refptr<CSSVariableData> value) {
    return new CSSCustomPropertyDeclaration(name, std::move(value));
  }

  static CSSCustomPropertyDeclaration* Create(const AtomicString& name,
                                              CSSValueID id) {
    return new CSSCustomPropertyDeclaration(name, id);
  }

  const AtomicString& GetName() const { return name_; }
  CSSVariableData* Value() const { return value_.get(); }

  bool IsInherit(bool is_inherited_property) const {
    return value_id_ == CSSValueInherit ||
           (is_inherited_property && value_id_ == CSSValueUnset);
  }
  bool IsInitial(bool is_inherited_property) const {
    return value_id_ == CSSValueInitial ||
           (!is_inherited_property && value_id_ == CSSValueUnset);
  }

  String CustomCSSText() const;

  bool Equals(const CSSCustomPropertyDeclaration& other) const {
    return this == &other;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSCustomPropertyDeclaration(const AtomicString& name, CSSValueID id)
      : CSSValue(kCustomPropertyDeclarationClass),
        name_(name),
        value_(nullptr),
        value_id_(id) {
    DCHECK(id == CSSValueInherit || id == CSSValueInitial ||
           id == CSSValueUnset);
  }

  CSSCustomPropertyDeclaration(const AtomicString& name,
                               scoped_refptr<CSSVariableData> value)
      : CSSValue(kCustomPropertyDeclarationClass),
        name_(name),
        value_(std::move(value)),
        value_id_(CSSValueInvalid) {}

  const AtomicString name_;
  scoped_refptr<CSSVariableData> value_;
  CSSValueID value_id_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCustomPropertyDeclaration,
                            IsCustomPropertyDeclaration());

}  // namespace blink

#endif  // CSSCustomPropertyDeclaration_h
