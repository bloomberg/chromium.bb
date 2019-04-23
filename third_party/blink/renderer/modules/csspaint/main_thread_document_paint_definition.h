// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_MAIN_THREAD_DOCUMENT_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_MAIN_THREAD_DOCUMENT_PAINT_DEFINITION_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSPaintDefinition;

// MainThreadDocumentPaintDefinition is a version of DocumentPaintDefinition for
// the OffMainThreadCSSPaint project. It is created on the worklet thread and
// then sent to the main thread, so makes a copy of the input CSSPaintDefinition
// data when created.
//
// MainThreadDocumentPaintDefinition consists of:
//   * A input properties which is a list of DOMStrings.
//   * A input argument syntaxes which is a list of parsed CSS Properties and
//     Values - not currently supported.
//   * A context alpha flag.
class MODULES_EXPORT MainThreadDocumentPaintDefinition {
 public:
  explicit MainThreadDocumentPaintDefinition(CSSPaintDefinition*);
  virtual ~MainThreadDocumentPaintDefinition();

  const Vector<CSSPropertyID>& NativeInvalidationProperties() const {
    return native_invalidation_properties_;
  }
  const Vector<String>& CustomInvalidationProperties() const {
    return custom_invalidation_properties_;
  }

  double alpha() const { return alpha_; }

 private:
  Vector<CSSPropertyID> native_invalidation_properties_;
  Vector<String> custom_invalidation_properties_;
  double alpha_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_MAIN_THREAD_DOCUMENT_PAINT_DEFINITION_H_
