// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorPropertyFunctions_h
#define ColorPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/css/StyleColor.h"

namespace blink {

class ComputedStyle;

struct OptionalStyleColor {
 public:
  OptionalStyleColor(std::nullptr_t) : is_null_(true) {}
  OptionalStyleColor(const StyleColor& style_color)
      : is_null_(false), style_color_(style_color) {}
  OptionalStyleColor(const Color& color)
      : is_null_(false), style_color_(color) {}

  bool IsNull() const { return is_null_; }
  const StyleColor& Access() const {
    DCHECK(!is_null_);
    return style_color_;
  }
  bool operator==(const OptionalStyleColor& other) const {
    return is_null_ == other.is_null_ && style_color_ == other.style_color_;
  }

 private:
  bool is_null_;
  StyleColor style_color_;
};

class ColorPropertyFunctions {
 public:
  static OptionalStyleColor GetInitialColor(CSSPropertyID);
  static OptionalStyleColor GetUnvisitedColor(CSSPropertyID,
                                              const ComputedStyle&);
  static OptionalStyleColor GetVisitedColor(CSSPropertyID,
                                            const ComputedStyle&);
  static void SetUnvisitedColor(CSSPropertyID, ComputedStyle&, const Color&);
  static void SetVisitedColor(CSSPropertyID, ComputedStyle&, const Color&);
};

}  // namespace blink

#endif  // ColorPropertyFunctions_h
