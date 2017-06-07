// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ActiveSuggestionMarker_h
#define ActiveSuggestionMarker_h

#include "core/editing/markers/StyleableMarker.h"

namespace blink {

// A subclass of StyleableMarker used to represent ActiveSuggestion markers,
// which are used to mark the region of text an open spellcheck or suggestion
// menu pertains to.
class CORE_EXPORT ActiveSuggestionMarker final : public StyleableMarker {
 public:
  ActiveSuggestionMarker(unsigned start_offset,
                         unsigned end_offset,
                         Color underline_color,
                         Thickness,
                         Color background_color);

  // DocumentMarker implementations
  MarkerType GetType() const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveSuggestionMarker);
};

DEFINE_TYPE_CASTS(ActiveSuggestionMarker,
                  DocumentMarker,
                  marker,
                  marker->GetType() == DocumentMarker::kActiveSuggestion,
                  marker.GetType() == DocumentMarker::kActiveSuggestion);

}  // namespace blink

#endif
