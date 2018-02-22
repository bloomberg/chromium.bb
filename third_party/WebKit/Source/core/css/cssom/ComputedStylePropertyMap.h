// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStylePropertyMap_h
#define ComputedStylePropertyMap_h

#include "base/macros.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSSelector.h"
#include "core/css/cssom/StylePropertyMapReadOnly.h"
#include "core/dom/Node.h"

namespace blink {

// This class implements computed StylePropertMapReadOnly in the Typed CSSOM
// API. The specification is here:
// https://drafts.css-houdini.org/css-typed-om-1/#computed-StylePropertyMapReadOnly-objects
//
// The computed StylePropertyMapReadOnly retrieves computed styles and returns
// them as CSSStyleValues. The IDL for this class is in StylePropertyMap.idl.
// The computed StylePropertyMapReadOnly for an element is accessed via
// element.computedStyleMap() (see ElementComputedStyleMap.idl/h)
class CORE_EXPORT ComputedStylePropertyMap : public StylePropertyMapReadOnly {
 public:
  static ComputedStylePropertyMap* Create(Node* node) {
    return new ComputedStylePropertyMap(node);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(node_);
    StylePropertyMapReadOnly::Trace(visitor);
  }

 protected:
  ComputedStylePropertyMap(Node* node, const String& pseudo_element = String())
      : StylePropertyMapReadOnly(),
        pseudo_id_(CSSSelector::ParsePseudoId(pseudo_element)),
        node_(node) {}

  const CSSValue* GetProperty(CSSPropertyID) override;
  const CSSValue* GetCustomProperty(AtomicString) override;
  void ForEachProperty(const IterationCallback&) override;

 private:
  // TODO: Pseudo-element support requires reintroducing Element.pseudo(...).
  // See
  // https://github.com/w3c/css-houdini-drafts/issues/350#issuecomment-294690156
  PseudoId pseudo_id_;
  Member<Node> node_;

  Node* StyledNode() const;
  const ComputedStyle* UpdateStyle();
  DISALLOW_COPY_AND_ASSIGN(ComputedStylePropertyMap);
};

}  // namespace blink

#endif
