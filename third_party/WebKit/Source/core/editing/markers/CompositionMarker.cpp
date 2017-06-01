// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/CompositionMarker.h"

namespace blink {

CompositionMarker::CompositionMarker(unsigned start_offset,
                                     unsigned end_offset,
                                     Color underline_color,
                                     bool thick,
                                     Color background_color)
    : DocumentMarker(DocumentMarker::kComposition, start_offset, end_offset),
      underline_color_(underline_color),
      background_color_(background_color),
      thick_(thick) {}

Color CompositionMarker::UnderlineColor() const {
  return underline_color_;
}

bool CompositionMarker::Thick() const {
  return thick_;
}

Color CompositionMarker::BackgroundColor() const {
  return background_color_;
}

}  // namespace blink
