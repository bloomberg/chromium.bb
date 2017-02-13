// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/BorderEdge.h"

namespace blink {

BorderEdge::BorderEdge(float edgeWidth,
                       const Color& edgeColor,
                       EBorderStyle edgeStyle,
                       bool edgeIsPresent)
    : color(edgeColor),
      isPresent(edgeIsPresent),
      style(edgeStyle),
      m_width(edgeWidth) {
  if (style == BorderStyleDouble && edgeWidth < 3)
    style = BorderStyleSolid;
}

BorderEdge::BorderEdge() : isPresent(false), style(BorderStyleHidden) {}

bool BorderEdge::hasVisibleColorAndStyle() const {
  return style > BorderStyleHidden && color.alpha() > 0;
}

bool BorderEdge::shouldRender() const {
  return isPresent && m_width && hasVisibleColorAndStyle();
}

bool BorderEdge::presentButInvisible() const {
  return usedWidth() && !hasVisibleColorAndStyle();
}

bool BorderEdge::obscuresBackgroundEdge() const {
  if (!isPresent || color.hasAlpha() || style == BorderStyleHidden)
    return false;

  if (style == BorderStyleDotted || style == BorderStyleDashed)
    return false;

  return true;
}

bool BorderEdge::obscuresBackground() const {
  if (!isPresent || color.hasAlpha() || style == BorderStyleHidden)
    return false;

  if (style == BorderStyleDotted || style == BorderStyleDashed ||
      style == BorderStyleDouble)
    return false;

  return true;
}

float BorderEdge::usedWidth() const {
  return isPresent ? m_width : 0;
}

float BorderEdge::getDoubleBorderStripeWidth(DoubleBorderStripe stripe) const {
  ASSERT(stripe == DoubleBorderStripeOuter ||
         stripe == DoubleBorderStripeInner);

  return roundf(stripe == DoubleBorderStripeOuter ? usedWidth() / 3
                                                  : (usedWidth() * 2) / 3);
}

bool BorderEdge::sharesColorWith(const BorderEdge& other) const {
  return color == other.color;
}

}  // namespace blink
