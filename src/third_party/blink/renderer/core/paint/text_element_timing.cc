// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_element_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
const char TextElementTiming::kSupplementName[] = "TextElementTiming";

// static
TextElementTiming& TextElementTiming::From(LocalDOMWindow& window) {
  TextElementTiming* timing =
      Supplement<LocalDOMWindow>::From<TextElementTiming>(window);
  if (!timing) {
    timing = MakeGarbageCollected<TextElementTiming>(window);
    ProvideTo(window, timing);
  }
  return *timing;
}

TextElementTiming::TextElementTiming(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {
  DCHECK(RuntimeEnabledFeatures::ElementTimingEnabled(
      GetSupplementable()->document()));
}

void TextElementTiming::OnTextNodesPainted(
    const Deque<base::WeakPtr<TextRecord>>& text_nodes_painted) {
  // TODO(npm): implement entry creation.
}

void TextElementTiming::Trace(blink::Visitor* visitor) {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
