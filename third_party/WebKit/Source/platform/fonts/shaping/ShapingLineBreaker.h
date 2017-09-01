// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapingLineBreaker_h
#define ShapingLineBreaker_h

#include "platform/LayoutUnit.h"
#include "platform/PlatformExport.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Font;
class ShapeResult;
class HarfBuzzShaper;
class Hyphenation;
class LazyLineBreakIterator;
enum class LineBreakType;
template <typename TextContainerType>
class ShapeResultSpacing;

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
  STACK_ALLOCATED();

 public:
  ShapingLineBreaker(const HarfBuzzShaper*,
                     const Font*,
                     const ShapeResult*,
                     const LazyLineBreakIterator*,
                     ShapeResultSpacing<String>* = nullptr,
                     const Hyphenation* = nullptr);
  ~ShapingLineBreaker() {}

  // Represents details of the result of |ShapeLine()|.
  struct Result {
    STACK_ALLOCATED();

    unsigned break_offset;

    // True if the break is hyphenated, either by automatic hyphenation or
    // soft-hyphen characters.
    // The hyphen glyph is not included in the |ShapeResult|, and that appending
    // a hyphen glyph may overflow the specified available space.
    bool is_hyphenated;

    // True if trailing spaces hang over the given |available_space|.
    // False when the result does not have trailing spaces, or trailing spaces
    // do not hang.
    bool has_hanging_spaces;
  };

  // Shapes a line of text by finding a valid and appropriate break opportunity
  // based on the shaping results for the entire paragraph.
  // The output parameter breakOffset indicates the resulting break offset.
  PassRefPtr<ShapeResult> ShapeLine(unsigned start_offset,
                                    LayoutUnit available_space,
                                    Result* result_out);

  // Disable breaking at soft hyphens (U+00AD).
  bool IsSoftHyphenEnabled() const { return is_soft_hyphen_enabled_; }
  void DisableSoftHyphen() { is_soft_hyphen_enabled_ = false; }

 private:
  const String& GetText() const;

  unsigned PreviousBreakOpportunity(unsigned offset,
                                    unsigned start,
                                    bool* is_hyphenated) const;
  unsigned NextBreakOpportunity(unsigned offset,
                                unsigned start,
                                bool* is_hyphenated) const;
  unsigned Hyphenate(unsigned offset,
                     unsigned start,
                     bool backwards,
                     bool* is_hyphenated) const;
  unsigned Hyphenate(unsigned offset,
                     unsigned word_start,
                     unsigned word_end,
                     bool backwards) const;

  PassRefPtr<ShapeResult> Shape(TextDirection, unsigned start, unsigned end);
  PassRefPtr<ShapeResult> ShapeToEnd(unsigned start,
                                     LayoutUnit start_position,
                                     unsigned range_end);

  const HarfBuzzShaper* shaper_;
  const Font* font_;
  const ShapeResult* result_;
  const LazyLineBreakIterator* break_iterator_;
  // TODO(kojii): ShapeResultSpacing is not const because it's stateful when it
  // has expansions. Split spacing and expansions to make this const.
  ShapeResultSpacing<String>* spacing_;
  const Hyphenation* hyphenation_;
  bool is_soft_hyphen_enabled_;

  friend class ShapingLineBreakerTest;
};

}  // namespace blink

#endif  // ShapingLineBreaker_h
