// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStylePropertyMap_h
#define ComputedStylePropertyMap_h

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/StylePropertyMapReadonly.h"
#include "core/dom/Node.h"
#include "core/layout/LayoutObject.h"

namespace blink {

// This class implements computed StylePropertMapReadOnly in the Typed CSSOM
// API. The specification is here:
// https://drafts.css-houdini.org/css-typed-om-1/#computed-stylepropertymapreadonly-objects
//
// The computed StylePropertyMapReadOnly retrieves computed styles and returns
// them as CSSStyleValues. The IDL for this class is in StylePropertyMap.idl.
// The computed StylePropertyMapReadOnly for an element is accessed via
// window.getComputedStyleMap(element) (see WindowGetComputedStyle.idl/h)
class CORE_EXPORT ComputedStylePropertyMap : public StylePropertyMapReadonly {
  WTF_MAKE_NONCOPYABLE(ComputedStylePropertyMap);

 public:
  static ComputedStylePropertyMap* Create(Node* node,
                                          const String& pseudo_element) {
    return new ComputedStylePropertyMap(node, pseudo_element);
  }

  Vector<String> getProperties() override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(node_);
    StylePropertyMapReadonly::Trace(visitor);
  }

 protected:
  ComputedStylePropertyMap(Node* node, const String& pseudo_element = String())
      : StylePropertyMapReadonly(),
        pseudo_id_(CSSSelector::ParsePseudoId(pseudo_element)),
        node_(node) {}

  CSSStyleValueVector GetAllInternal(CSSPropertyID) override;
  CSSStyleValueVector GetAllInternal(
      AtomicString custom_property_name) override;

  HeapVector<StylePropertyMapEntry> GetIterationEntries() override {
    return HeapVector<StylePropertyMapEntry>();
  }

  PseudoId pseudo_id_;
  Member<Node> node_;

 private:
  Node* StyledNode() const;
  const ComputedStyle* UpdateStyle();
};

}  // namespace blink

#endif
