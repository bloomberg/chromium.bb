// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_MAIN_THREAD_DOCUMENT_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_MAIN_THREAD_DOCUMENT_PAINT_DEFINITION_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// MainThreadDocumentPaintDefinition is a version of DocumentPaintDefinition for
// the OffMainThreadCSSPaint project. It is created on the main thread, using a
// copied version of native and custom invalidation properties.
class MODULES_EXPORT MainThreadDocumentPaintDefinition {
 public:
  explicit MainThreadDocumentPaintDefinition(
      const Vector<CSSPropertyID>& native_invalidation_properties,
      const Vector<String>& custom_invalidation_properties,
      const Vector<CSSSyntaxDescriptor>& input_argument_types,
      bool alpha);
  virtual ~MainThreadDocumentPaintDefinition();

  const Vector<CSSPropertyID>& NativeInvalidationProperties() const {
    return native_invalidation_properties_;
  }
  // Use AtomicString so that it is consistent with CSSPaintImageGeneratorImpl.
  const Vector<AtomicString>& CustomInvalidationProperties() const {
    return custom_invalidation_properties_;
  }

  const Vector<CSSSyntaxDescriptor>& InputArgumentTypes() const {
    return input_argument_types_;
  }

  bool alpha() const { return alpha_; }

 private:
  Vector<CSSPropertyID> native_invalidation_properties_;
  Vector<AtomicString> custom_invalidation_properties_;
  Vector<CSSSyntaxDescriptor> input_argument_types_;
  bool alpha_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_MAIN_THREAD_DOCUMENT_PAINT_DEFINITION_H_
