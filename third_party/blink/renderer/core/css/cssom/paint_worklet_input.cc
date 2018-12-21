// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"

namespace blink {

PaintWorkletInput::PaintWorkletInput(
    const std::string& name,
    const FloatSize& container_size,
    float effective_zoom,
    const Document& document,
    const ComputedStyle& style,
    Node* styled_node,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties)
    : name_(name),
      container_size_(container_size),
      effective_zoom_(effective_zoom),
      style_map_(MakeGarbageCollected<PaintWorkletStylePropertyMap>(
          document,
          style,
          styled_node,
          native_properties,
          custom_properties)) {}

}  // namespace blink
