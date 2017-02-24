// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStylePropertyMap_h
#define ComputedStylePropertyMap_h

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/ImmutableStylePropertyMap.h"
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
class CORE_EXPORT ComputedStylePropertyMap : public ImmutableStylePropertyMap {
  WTF_MAKE_NONCOPYABLE(ComputedStylePropertyMap);

 public:
  static ComputedStylePropertyMap* create(Node* node,
                                          const String& pseudoElement) {
    return new ComputedStylePropertyMap(node, pseudoElement);
  }

  Vector<String> getProperties() override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_computedStyleDeclaration);
    visitor->trace(m_node);
    ImmutableStylePropertyMap::trace(visitor);
  }

 private:
  Node* node() const;

 protected:
  ComputedStylePropertyMap(Node* node, const String& pseudoElement = String())
      : ImmutableStylePropertyMap(),
        m_computedStyleDeclaration(
            CSSComputedStyleDeclaration::create(node, false, pseudoElement)),
        m_pseudoId(CSSSelector::parsePseudoId(pseudoElement)),
        m_node(node) {}

  CSSStyleValueVector getAllInternal(CSSPropertyID) override;
  CSSStyleValueVector getAllInternal(AtomicString customPropertyName) override;

  HeapVector<StylePropertyMapEntry> getIterationEntries() override {
    return HeapVector<StylePropertyMapEntry>();
  }

  Member<CSSComputedStyleDeclaration> m_computedStyleDeclaration;
  PseudoId m_pseudoId;
  Member<Node> m_node;
};

}  // namespace blink

#endif
