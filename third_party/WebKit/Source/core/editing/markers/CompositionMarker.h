// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositionMarker_h
#define CompositionMarker_h

#include "core/editing/markers/DocumentMarker.h"

namespace blink {

// A subclass of DocumentMarker used to store information specific to
// composition markers. We store what color to display the underline (possibly
// transparent), whether the underline should be thick or not, and what
// background color should be used under the marked text (also possibly
// transparent).
class CORE_EXPORT CompositionMarker final : public DocumentMarker {
 public:
  CompositionMarker(unsigned start_offset,
                    unsigned end_offset,
                    Color underline_color,
                    bool thick,
                    Color background_color);

  // DocumentMarker implementations
  MarkerType GetType() const final;

  // CompositionMarker-specific
  Color UnderlineColor() const;
  bool Thick() const;
  Color BackgroundColor() const;

 private:
  const Color underline_color_;
  const Color background_color_;
  const bool thick_;

  DISALLOW_COPY_AND_ASSIGN(CompositionMarker);
};

DEFINE_TYPE_CASTS(CompositionMarker,
                  DocumentMarker,
                  marker,
                  marker->GetType() == DocumentMarker::kComposition,
                  marker.GetType() == DocumentMarker::kComposition);

}  // namespace blink

#endif
