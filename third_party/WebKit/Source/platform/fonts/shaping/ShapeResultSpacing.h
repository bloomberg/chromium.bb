// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultSpacing_h
#define ShapeResultSpacing_h

#include "platform/PlatformExport.h"
#include "platform/text/Character.h"

namespace blink {

class FontDescription;
class TextRun;

class PLATFORM_EXPORT ShapeResultSpacing final {
 public:
  ShapeResultSpacing(const TextRun&, const FontDescription&);

  float LetterSpacing() const { return letter_spacing_; }
  bool HasSpacing() const { return has_spacing_; }
  bool IsVerticalOffset() const { return is_vertical_offset_; }

  float ComputeSpacing(const TextRun&, size_t, float& offset);

 private:
  bool HasExpansion() const { return expansion_opportunity_count_; }
  bool IsAfterExpansion() const { return is_after_expansion_; }
  bool IsFirstRun(const TextRun&) const;

  float NextExpansion();

  const TextRun& text_run_;
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
