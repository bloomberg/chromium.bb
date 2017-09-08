/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef LayoutSVGGradientStop_h
#define LayoutSVGGradientStop_h

#include "core/layout/LayoutObject.h"

namespace blink {

class SVGGradientElement;
class SVGStopElement;

// This class exists mostly so we can hear about gradient stop style changes
class LayoutSVGGradientStop final : public LayoutObject {
 public:
  explicit LayoutSVGGradientStop(SVGStopElement*);
  ~LayoutSVGGradientStop() override;

  const char* GetName() const override { return "LayoutSVGGradientStop"; }
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVG || type == kLayoutObjectSVGGradientStop ||
           LayoutObject::IsOfType(type);
  }

  void UpdateLayout() override;

 private:
  // These overrides are needed to prevent NOTREACHED on <svg><stop /></svg> in
  // LayoutObject's default implementations.
  LayoutRect LocalVisualRectIgnoringVisibility() const final {
    return LayoutRect();
  }
  FloatRect ObjectBoundingBox() const final { return FloatRect(); }
  FloatRect StrokeBoundingBox() const final { return FloatRect(); }
  FloatRect VisualRectInLocalSVGCoordinates() const final {
    return FloatRect();
  }
  FloatRect LocalBoundingBoxRectForAccessibility() const final {
    return FloatRect();
  }

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) final;

  SVGGradientElement* GradientElement() const;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGGradientStop, IsSVGGradientStop());

}  // namespace blink

#endif  // LayoutSVGGradientStop_h
