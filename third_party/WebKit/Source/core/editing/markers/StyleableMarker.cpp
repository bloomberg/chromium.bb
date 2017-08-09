// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/StyleableMarker.h"

namespace blink {

StyleableMarker::StyleableMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 Color underline_color,
                                 Thickness thickness,
                                 Color background_color)
    : DocumentMarker(start_offset, end_offset),
      underline_color_(underline_color),
      background_color_(background_color),
      thickness_(thickness) {}

Color StyleableMarker::UnderlineColor() const {
  return underline_color_;
}

bool StyleableMarker::IsThick() const {
  return thickness_ == Thickness::kThick;
}

Color StyleableMarker::BackgroundColor() const {
  return background_color_;
}

bool IsStyleableMarker(const DocumentMarker& marker) {
  DocumentMarker::MarkerType type = marker.GetType();
  return type == DocumentMarker::kComposition ||
         type == DocumentMarker::kActiveSuggestion ||
         type == DocumentMarker::kSuggestion;
}

}  // namespace blink
