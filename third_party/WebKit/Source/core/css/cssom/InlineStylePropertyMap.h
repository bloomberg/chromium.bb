// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineStylePropertyMap_h
#define InlineStylePropertyMap_h

#include "base/macros.h"
#include "core/css/cssom/StylePropertyMap.h"
#include "core/dom/Element.h"

namespace blink {

class CORE_EXPORT InlineStylePropertyMap final : public StylePropertyMap {
 public:
  explicit InlineStylePropertyMap(Element* owner_element)
      : owner_element_(owner_element) {}

  Vector<String> getProperties() override;

  void set(const ExecutionContext*,
           CSSPropertyID,
           HeapVector<CSSStyleValueOrString>&,
           ExceptionState&) override;
  void append(const ExecutionContext*,
              CSSPropertyID,
              HeapVector<CSSStyleValueOrString>&,
              ExceptionState&) override;
  void remove(CSSPropertyID, ExceptionState&) override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(owner_element_);
    StylePropertyMap::Trace(visitor);
  }

 protected:
  const CSSValue* GetProperty(CSSPropertyID) override;
  const CSSValue* GetCustomProperty(AtomicString) override;

  HeapVector<StylePropertyMapEntry> GetIterationEntries() override;

 private:
  Member<Element> owner_element_;
  DISALLOW_COPY_AND_ASSIGN(InlineStylePropertyMap);
};

}  // namespace blink

#endif
