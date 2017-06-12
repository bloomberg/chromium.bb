// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultSpacing_h
#define ShapeResultSpacing_h

#include "platform/PlatformExport.h"
#include "platform/text/Character.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class FontDescription;

// A context object to apply letter-spacing, word-spacing, and justification to
// ShapeResult.
template <typename TextContainerType>
class PLATFORM_EXPORT ShapeResultSpacing final {
  STACK_ALLOCATED();

 public:
  ShapeResultSpacing(const TextContainerType&);

  float LetterSpacing() const { return letter_spacing_; }
  bool HasSpacing() const { return has_spacing_; }
  bool IsVerticalOffset() const { return is_vertical_offset_; }

  // Set letter-spacing and word-spacing.
  bool SetSpacing(const FontDescription&);

  // Set letter-spacing, word-spacing, and justification.
  // Available only for TextRun.
  void SetSpacingAndExpansion(const FontDescription&);

  // Compute the sum of all spacings for the specified index.
  // For justification, this function must be called incrementally since it
  // keeps states and counts consumed justification opportunities.
  float ComputeSpacing(const TextContainerType&, size_t, float& offset);

 private:
  bool HasExpansion() const { return expansion_opportunity_count_; }
  bool IsAfterExpansion() const { return is_after_expansion_; }
  bool IsFirstRun(const TextContainerType&) const;

  void ComputeExpansion(bool allows_leading_expansion,
                        bool allows_trailing_expansion,
                        TextJustify);

  float NextExpansion();

  const TextContainerType& text_;
  float letter_spacing_;
  float word_spacing_;
  float expansion_;
  float expansion_per_opportunity_;
  unsigned expansion_opportunity_count_;
  TextJustify text_justify_;
  bool has_spacing_;
  bool normalize_space_;
  bool allow_tabs_;
  bool is_after_expansion_;
  bool is_vertical_offset_;
};

}  // namespace blink

#endif
