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

#include "third_party/blink/renderer/core/editing/markers/highlight_marker.h"

namespace blink {

HighlightMarker::HighlightMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 Color foreground_color,
                                 Color background_color,
                                 bool include_nonselectable_text)
    : DocumentMarker(start_offset, end_offset),
    	foreground_color(foreground_color),
    	background_color(background_color),
    	include_nonselectable_text(include_nonselectable_text) {}

DocumentMarker::MarkerType HighlightMarker::GetType() const {
  return DocumentMarker::kHighlight;
}

Color HighlightMarker::ForegroundColor() const {
  return foreground_color;
}

Color HighlightMarker::BackgroundColor() const {
  return background_color;
}

bool HighlightMarker::IncludeNonSelectableText() const {
  return include_nonselectable_text;
}

}  // namespace blink
