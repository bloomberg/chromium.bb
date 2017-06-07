// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/ActiveSuggestionMarker.h"

namespace blink {

ActiveSuggestionMarker::ActiveSuggestionMarker(unsigned start_offset,
                                               unsigned end_offset,
                                               Color underline_color,
                                               Thickness thickness,
                                               Color background_color)
    : StyleableMarker(start_offset,
                      end_offset,
                      underline_color,
                      thickness,
                      background_color) {}

DocumentMarker::MarkerType ActiveSuggestionMarker::GetType() const {
  return DocumentMarker::kActiveSuggestion;
}

}  // namespace blink
