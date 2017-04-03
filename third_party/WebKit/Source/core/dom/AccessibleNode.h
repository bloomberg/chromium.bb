// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AccessibleNode_h
#define AccessibleNode_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class Element;

// All of the properties of AccessibleNode that have type "string".
// TODO(dmazzoni): Add similar enums for all of the properties with
// type bool, float, reference, and reference list.
enum class AOMStringProperty { kRole, kLabel };

// Accessibility Object Model node
// Explainer: https://github.com/WICG/aom/blob/master/explainer.md
// Spec: https://wicg.github.io/aom/spec/
class CORE_EXPORT AccessibleNode
    : public GarbageCollectedFinalized<AccessibleNode>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AccessibleNode(Element*);
  virtual ~AccessibleNode();

  // Returns the given string property if the Element has an AccessibleNode,
  // otherwise returns the equivalent ARIA attribute.
  static const AtomicString& getProperty(Element*, AOMStringProperty);

  AtomicString role() const;
  void setRole(const AtomicString&);

  AtomicString label() const;
  void setLabel(const AtomicString&);

  DECLARE_VIRTUAL_TRACE();

 private:
  void setStringProperty(AOMStringProperty, const AtomicString&);

  Vector<std::pair<AOMStringProperty, AtomicString>> m_stringProperties;

  // This object's owner Element.
  Member<Element> m_element;
};

}  // namespace blink

#endif  // AccessibleNode_h
