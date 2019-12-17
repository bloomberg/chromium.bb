/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#ifndef HighlightMarker_h
#define HighlightMarker_h

#include "third_party/blink/renderer/core/editing/markers/document_marker.h"

namespace blink {

// A subclass of DocumentMarker used to store information specific to Highlight
// markers. We store whether or not the match is active, a LayoutRect used for
// rendering the marker, and whether or not the LayoutRect is currently
// up-to-date.
class CORE_EXPORT HighlightMarker final : public DocumentMarker {
 public:
  HighlightMarker(unsigned start_offset,
                  unsigned end_offset,
                  Color foreground_color,
                  Color background_color,
									bool include_nonselectable_text);

  // DocumentMarker implementations
  MarkerType GetType() const final;

  // HighlightMarker-specific implementations
  Color ForegroundColor() const;
  Color BackgroundColor() const;
  bool IncludeNonSelectableText() const;

 private:
	Color foreground_color;
	Color background_color;
	bool include_nonselectable_text = false;
  DISALLOW_COPY_AND_ASSIGN(HighlightMarker);
};

DEFINE_TYPE_CASTS(HighlightMarker,
                  DocumentMarker,
                  marker,
                  marker->GetType() == DocumentMarker::kHighlight,
                  marker.GetType() == DocumentMarker::kHighlight);

}  // namespace blink

#endif
