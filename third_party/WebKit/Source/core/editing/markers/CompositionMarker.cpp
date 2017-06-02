// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/CompositionMarker.h"

namespace blink {

CompositionMarker::CompositionMarker(unsigned start_offset,
                                     unsigned end_offset,
                                     Color underline_color,
                                     Thickness thickness,
                                     Color background_color)
    : DocumentMarker(start_offset, end_offset),
      underline_color_(underline_color),
      background_color_(background_color),
      thickness_(thickness) {}

DocumentMarker::MarkerType CompositionMarker::GetType() const {
  return DocumentMarker::kComposition;
}

Color CompositionMarker::UnderlineColor() const {
  return underline_color_;
}

bool CompositionMarker::IsThick() const {
  return thickness_ == Thickness::kThick;
}

Color CompositionMarker::BackgroundColor() const {
  return background_color_;
}

}  // namespace blink
