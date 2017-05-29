// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelList_h
#define RelList_h

#include "core/HTMLNames.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/Element.h"
#include "core/dom/SpaceSplitString.h"

namespace blink {

class RelList final : public DOMTokenList {
 public:
  static RelList* Create(Element* element) { return new RelList(element); }
  DECLARE_VIRTUAL_TRACE();

 private:
  explicit RelList(Element*);

  void setValue(const AtomicString& value) override {
    element_->setAttribute(HTMLNames::relAttr, value);
  }

  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;

  Member<Element> element_;
};

}  // namespace blink

#endif  // RelList_h
