// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/highlight_marker.h"

#include "third_party/blink/renderer/core/highlight/highlight.h"

namespace blink {

HighlightMarker::HighlightMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 const String& highlight_name,
                                 const Member<Highlight> highlight)
    : DocumentMarker(start_offset, end_offset),
      highlight_name_(highlight_name),
      highlight_(highlight) {}

DocumentMarker::MarkerType HighlightMarker::GetType() const {
  return DocumentMarker::kHighlight;
}

void HighlightMarker::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlight_);
  DocumentMarker::Trace(visitor);
}

}  // namespace blink
