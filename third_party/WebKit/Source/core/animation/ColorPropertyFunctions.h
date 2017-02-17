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
  OptionalStyleColor(std::nullptr_t) : m_isNull(true) {}
  OptionalStyleColor(const StyleColor& styleColor)
      : m_isNull(false), m_styleColor(styleColor) {}
  OptionalStyleColor(const Color& color)
      : m_isNull(false), m_styleColor(color) {}

  bool isNull() const { return m_isNull; }
  const StyleColor& access() const {
    DCHECK(!m_isNull);
    return m_styleColor;
  }
  bool operator==(const OptionalStyleColor& other) const {
    return m_isNull == other.m_isNull && m_styleColor == other.m_styleColor;
  }

 private:
  bool m_isNull;
  StyleColor m_styleColor;
};

class ColorPropertyFunctions {
 public:
  static OptionalStyleColor getInitialColor(CSSPropertyID);
  static OptionalStyleColor getUnvisitedColor(CSSPropertyID,
                                              const ComputedStyle&);
  static OptionalStyleColor getVisitedColor(CSSPropertyID,
                                            const ComputedStyle&);
  static void setUnvisitedColor(CSSPropertyID, ComputedStyle&, const Color&);
  static void setVisitedColor(CSSPropertyID, ComputedStyle&, const Color&);
};

}  // namespace blink

#endif  // ColorPropertyFunctions_h
