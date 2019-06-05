// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_element_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
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
    : Supplement<LocalDOMWindow>(window),
      performance_(DOMWindowPerformance::performance(window)) {
  DCHECK(RuntimeEnabledFeatures::ElementTimingEnabled(
      GetSupplementable()->document()));
}

void TextElementTiming::OnTextNodesPainted(
    const Deque<base::WeakPtr<TextRecord>>& text_nodes_painted) {
  DCHECK(performance_);
  // If the entries cannot be exposed via PerformanceObserver nor added to the
  // buffer, bail out.
  if (!performance_->HasObserverFor(PerformanceEntry::kElement) &&
      performance_->IsElementTimingBufferFull()) {
    return;
  }
  for (const auto record : text_nodes_painted) {
    Node* node = DOMNodeIds::NodeForId(record->node_id);
    if (!node || node->IsInShadowTree())
      continue;

    // Text aggregators should be Elements!
    DCHECK(node->IsElementNode());
    Element* element = ToElement(node);
    const AtomicString& attr =
        element->FastGetAttribute(html_names::kElementtimingAttr);
    if (attr.IsEmpty())
      continue;

    const AtomicString& id = element->GetIdAttribute();
    DEFINE_STATIC_LOCAL(const AtomicString, kTextPaint, ("text-paint"));
    // TODO(npm): Add the rect once these are stored in TextRecord.
    performance_->AddElementTiming(kTextPaint, g_empty_string, FloatRect(),
                                   record->paint_time, TimeTicks(), attr,
                                   IntSize(), id, element);
  }
}

void TextElementTiming::Trace(blink::Visitor* visitor) {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(performance_);
}

}  // namespace blink
