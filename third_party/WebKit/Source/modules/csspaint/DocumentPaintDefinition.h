// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentPaintDefinition_h
#define DocumentPaintDefinition_h

#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

// A document paint definition is a struct which describes the information
// needed by the document about the author defined image function (which can be
// referenced by the paint function). It consists of:
//   * A input properties which is a list of DOMStrings.
//   * A input argument syntaxes which is a list of parsed CSS Properties and
//     Values.
//   * A context alpha flag.
//
// The document has a map of document paint definitions. Initially the map is
// empty; it is populated when registerPaint(name, paintCtor) is called.
class DocumentPaintDefinition final
    : public GarbageCollectedFinalized<DocumentPaintDefinition>,
      public TraceWrapperBase {
 public:
  explicit DocumentPaintDefinition(CSSPaintDefinition*);
  virtual ~DocumentPaintDefinition();

  const Vector<CSSPropertyID>& NativeInvalidationProperties() const {
    return paint_definition_->NativeInvalidationProperties();
  }
  const Vector<AtomicString>& CustomInvalidationProperties() const {
    return paint_definition_->CustomInvalidationProperties();
  }
  const Vector<CSSSyntaxDescriptor>& InputArgumentTypes() const {
    return paint_definition_->InputArgumentTypes();
  }
  PaintRenderingContext2DSettings GetPaintRenderingContext2DSettings() const {
    return paint_definition_->GetPaintRenderingContext2DSettings();
  }

  bool RegisterAdditionalPaintDefinition(const CSSPaintDefinition&);

  unsigned GetRegisteredDefinitionCount() const {
    return registered_definitions_count_;
  }

  virtual void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor* visitor) const {};

 private:
  Member<CSSPaintDefinition> paint_definition_;
  unsigned registered_definitions_count_;
};

}  // namespace blink

#endif  // DocumentPaintDefinition_h
