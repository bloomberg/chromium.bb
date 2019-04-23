// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/main_thread_document_paint_definition.h"

#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"

namespace blink {

MainThreadDocumentPaintDefinition::MainThreadDocumentPaintDefinition(
    CSSPaintDefinition* definition)
    : native_invalidation_properties_(
          definition->NativeInvalidationProperties()),
      alpha_(definition->GetPaintRenderingContext2DSettings()->alpha()) {
  // MainThreadDocumentPaintDefinition is sent cross-thread from the
  // PaintWorklet thread to the main thread, so we have to make isolated copies
  // of the custom properties.
  const Vector<AtomicString>& custom_invalidation_properties =
      definition->CustomInvalidationProperties();
  custom_invalidation_properties_.ReserveInitialCapacity(
      custom_invalidation_properties.size());
  for (const AtomicString& property : custom_invalidation_properties) {
    custom_invalidation_properties_.push_back(
        property.GetString().IsolatedCopy());
  }
}

MainThreadDocumentPaintDefinition::~MainThreadDocumentPaintDefinition() =
    default;

}  // namespace blink
