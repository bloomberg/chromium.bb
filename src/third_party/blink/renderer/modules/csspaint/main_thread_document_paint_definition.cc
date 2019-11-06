// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/main_thread_document_paint_definition.h"

#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"

namespace blink {

MainThreadDocumentPaintDefinition::MainThreadDocumentPaintDefinition(
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<String>& custom_invalidation_properties,
    bool alpha)
    : native_invalidation_properties_(native_invalidation_properties),
      alpha_(alpha) {
  custom_invalidation_properties_.ReserveInitialCapacity(
      custom_invalidation_properties.size());
  for (const String& property : custom_invalidation_properties)
    custom_invalidation_properties_.push_back(AtomicString(property));
}

MainThreadDocumentPaintDefinition::~MainThreadDocumentPaintDefinition() =
    default;

}  // namespace blink
