// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineStylePropertyMap_h
#define InlineStylePropertyMap_h

#include "core/css/cssom/StylePropertyMap.h"
#include "core/dom/Element.h"

namespace blink {

class CORE_EXPORT InlineStylePropertyMap final : public StylePropertyMap {
  WTF_MAKE_NONCOPYABLE(InlineStylePropertyMap);

 public:
  explicit InlineStylePropertyMap(Element* owner_element)
      : owner_element_(owner_element) {}

  Vector<String> getProperties() override;

  void set(CSSPropertyID,
           CSSStyleValueOrCSSStyleValueSequenceOrString&,
           ExceptionState&) override;
  void append(CSSPropertyID,
              CSSStyleValueOrCSSStyleValueSequenceOrString&,
              ExceptionState&) override;
  void remove(CSSPropertyID, ExceptionState&) override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(owner_element_);
    StylePropertyMap::Trace(visitor);
  }

 protected:
  CSSStyleValueVector GetAllInternal(CSSPropertyID) override;
  CSSStyleValueVector GetAllInternal(
      AtomicString custom_property_name) override;

  HeapVector<StylePropertyMapEntry> GetIterationEntries() override;

 private:
  Member<Element> owner_element_;
};

}  // namespace blink

#endif
