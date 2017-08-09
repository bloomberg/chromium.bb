// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SuggestionMarker.h"

namespace blink {

int32_t SuggestionMarker::current_tag_ = 0;

SuggestionMarker::SuggestionMarker(unsigned start_offset,
                                   unsigned end_offset,
                                   const Vector<String>& suggestions,
                                   Color suggestion_highlight_color,
                                   Color underline_color,
                                   Thickness thickness,
                                   Color background_color)
    : StyleableMarker(start_offset,
                      end_offset,
                      underline_color,
                      thickness,
                      background_color),
      tag_(NextTag()),
      suggestions_(suggestions),
      suggestion_highlight_color_(suggestion_highlight_color) {
  DCHECK_GT(tag_, 0);
}

int32_t SuggestionMarker::Tag() const {
  return tag_;
}

DocumentMarker::MarkerType SuggestionMarker::GetType() const {
  return DocumentMarker::kSuggestion;
}

const Vector<String>& SuggestionMarker::Suggestions() const {
  return suggestions_;
}

Color SuggestionMarker::SuggestionHighlightColor() const {
  return suggestion_highlight_color_;
}

void SuggestionMarker::SetSuggestion(uint32_t suggestion_index,
                                     const String& new_suggestion) {
  DCHECK_LT(suggestion_index, suggestions_.size());
  suggestions_[suggestion_index] = new_suggestion;
}

// static
int32_t SuggestionMarker::NextTag() {
  return ++current_tag_;
}

}  // namespace blink
