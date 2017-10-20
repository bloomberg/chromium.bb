// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleNonInheritedVariables_h
#define StyleNonInheritedVariables_h

#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class StyleNonInheritedVariables {
 public:
  static std::unique_ptr<StyleNonInheritedVariables> Create() {
    return WTF::WrapUnique(new StyleNonInheritedVariables);
  }

  std::unique_ptr<StyleNonInheritedVariables> Clone() {
    return WTF::WrapUnique(new StyleNonInheritedVariables(*this));
  }

  bool operator==(const StyleNonInheritedVariables& other) const;
  bool operator!=(const StyleNonInheritedVariables& other) const {
    return !(*this == other);
  }

  void SetVariable(const AtomicString& name,
                   scoped_refptr<CSSVariableData> value) {
    data_.Set(name, std::move(value));
  }
  CSSVariableData* GetVariable(const AtomicString& name) const;
  void RemoveVariable(const AtomicString&);

  void SetRegisteredVariable(const AtomicString&, const CSSValue*);
  const CSSValue* RegisteredVariable(const AtomicString& name) const {
    return registered_data_.at(name);
  }

 private:
  StyleNonInheritedVariables() = default;
  StyleNonInheritedVariables(StyleNonInheritedVariables&);

  friend class CSSVariableResolver;

  HashMap<AtomicString, scoped_refptr<CSSVariableData>> data_;
  HashMap<AtomicString, Persistent<CSSValue>> registered_data_;
};

}  // namespace blink

#endif  // StyleNonInheritedVariables_h
