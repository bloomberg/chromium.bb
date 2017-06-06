// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleableMarker_h
#define StyleableMarker_h

#include "core/editing/markers/DocumentMarker.h"

namespace blink {

// An abstract subclass of DocumentMarker to be subclassed by DocumentMarkers
// that should be renderable with customizable formatting.
class CORE_EXPORT StyleableMarker : public DocumentMarker {
 public:
  enum class Thickness { kThin, kThick };

  StyleableMarker(unsigned start_offset,
                  unsigned end_offset,
                  Color underline_color,
                  Thickness,
                  Color background_color);

  // StyleableMarker-specific
  Color UnderlineColor() const;
  bool IsThick() const;
  Color BackgroundColor() const;

 private:
  const Color underline_color_;
  const Color background_color_;
  const Thickness thickness_;

  DISALLOW_COPY_AND_ASSIGN(StyleableMarker);
};

bool CORE_EXPORT IsStyleableMarker(const DocumentMarker&);

DEFINE_TYPE_CASTS(StyleableMarker,
                  DocumentMarker,
                  marker,
                  IsStyleableMarker(*marker),
                  IsStyleableMarker(marker));

}  // namespace blink

#endif
