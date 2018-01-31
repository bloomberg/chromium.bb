// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentLayoutDefinition_h
#define DocumentLayoutDefinition_h

#include "core/layout/custom/CSSLayoutDefinition.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

// A document layout definition is a struct which describes the information
// needed by the document about the author defined layout.
// https://drafts.css-houdini.org/css-layout-api/#document-layout-definition
class DocumentLayoutDefinition final
    : public GarbageCollectedFinalized<DocumentLayoutDefinition> {
 public:
  explicit DocumentLayoutDefinition(CSSLayoutDefinition*);
  virtual ~DocumentLayoutDefinition();

  const Vector<CSSPropertyID>& NativeInvalidationProperties() const {
    return layout_definition_->NativeInvalidationProperties();
  }
  const Vector<AtomicString>& CustomInvalidationProperties() const {
    return layout_definition_->CustomInvalidationProperties();
  }
  const Vector<CSSPropertyID>& ChildNativeInvalidationProperties() const {
    return layout_definition_->ChildNativeInvalidationProperties();
  }
  const Vector<AtomicString>& ChildCustomInvalidationProperties() const {
    return layout_definition_->ChildCustomInvalidationProperties();
  }

  bool RegisterAdditionalLayoutDefinition(const CSSLayoutDefinition&);

  unsigned GetRegisteredDefinitionCount() const {
    return registered_definitions_count_;
  }

  virtual void Trace(blink::Visitor*);

 private:
  bool IsEqual(const CSSLayoutDefinition&);

  Member<CSSLayoutDefinition> layout_definition_;
  unsigned registered_definitions_count_;
};

}  // namespace blink

#endif  // DocumentLayoutDefinition_h
