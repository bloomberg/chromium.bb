// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapingLineBreaker_h
#define ShapingLineBreaker_h

#include "platform/LayoutUnit.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Font;
class ShapeResult;
class HarfBuzzShaper;
class LazyLineBreakIterator;
enum class LineBreakType;

// Shapes a line of text by finding the ideal break position as indicated by the
// available space and the shape results for the entire paragraph. Once an ideal
// break position has been found the text is scanned backwards until a valid and
// and appropriate break opportunity is identified. Unless the break opportunity
// is at a safe-to-break boundary (as identified by HarfBuzz) the beginning and/
// or end of the line is reshaped to account for differences caused by breaking.
//
// This allows for significantly faster and more efficient line breaking by only
// reshaping when absolutely necessarily and by only evaluating likely candidate
// break opportunities instead of measuring and evaluating all possible options.
class PLATFORM_EXPORT ShapingLineBreaker final {
 public:
  ShapingLineBreaker(const HarfBuzzShaper*,
                     const Font*,
                     const ShapeResult*,
                     const AtomicString,
                     LineBreakType);
  ~ShapingLineBreaker() {}

  // Shapes a line of text by finding a valid and appropriate break opportunity
  // based on the shaping results for the entire paragraph.
  // The output parameter breakOffset indicates the resulting break offset.
  PassRefPtr<ShapeResult> shapeLine(unsigned startOffset,
                                    LayoutUnit availableSpace,
                                    unsigned* breakOffset);

 private:
  unsigned previousBreakOpportunity(LazyLineBreakIterator*,
                                    unsigned start,
                                    unsigned offset);
  unsigned nextBreakOpportunity(LazyLineBreakIterator*, unsigned offset);

  const HarfBuzzShaper* m_shaper;
  const Font* m_font;
  const ShapeResult* m_result;
  const AtomicString m_locale;
  LineBreakType m_breakType;
  String m_text;
};

}  // namespace blink

#endif  // ShapingLineBreaker_h
