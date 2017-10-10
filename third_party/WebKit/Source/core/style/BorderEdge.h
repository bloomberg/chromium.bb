// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BorderEdge_h
#define BorderEdge_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/Allocator.h"

namespace blink {

struct BorderEdge {
  STACK_ALLOCATED();

 public:
  BorderEdge(float edge_width,
             const Color& edge_color,
             EBorderStyle edge_style,
             bool edge_is_present = true);
  BorderEdge();

  bool HasVisibleColorAndStyle() const;
  bool ShouldRender() const;
  bool PresentButInvisible() const;
  bool ObscuresBackgroundEdge() const;
  bool ObscuresBackground() const;
  float UsedWidth() const;

  bool SharesColorWith(const BorderEdge& other) const;

  EBorderStyle BorderStyle() const { return static_cast<EBorderStyle>(style); }

  enum DoubleBorderStripe {
    kDoubleBorderStripeOuter,
    kDoubleBorderStripeInner
  };

  float GetDoubleBorderStripeWidth(DoubleBorderStripe) const;

  float Width() const { return width_; }

  void ClampWidth(float width) {
    if (width_ > width)
      width_ = width;
  }

  Color color;
  bool is_present;

 private:
  unsigned style : 4;  // EBorderStyle
  float width_;
};

}  // namespace blink

#endif  // BorderEdge_h
