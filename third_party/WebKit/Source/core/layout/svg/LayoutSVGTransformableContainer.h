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

#ifndef LayoutSVGTransformableContainer_h
#define LayoutSVGTransformableContainer_h

#include "core/layout/svg/LayoutSVGContainer.h"

namespace blink {

class SVGGraphicsElement;

class LayoutSVGTransformableContainer final : public LayoutSVGContainer {
 public:
  explicit LayoutSVGTransformableContainer(SVGGraphicsElement*);

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGTransformableContainer ||
           LayoutSVGContainer::IsOfType(type);
  }
  const FloatSize& AdditionalTranslation() const {
    return additional_translation_;
  }

  void SetNeedsTransformUpdate() override;

 private:
  SVGTransformChange CalculateLocalTransform() override;
  AffineTransform LocalSVGTransform() const override {
    return local_transform_;
  }
  bool IsUseElement() const;

  bool needs_transform_update_ : 1;
  AffineTransform local_transform_;
  FloatSize additional_translation_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGTransformableContainer,
                                IsSVGTransformableContainer());

}  // namespace blink

#endif  // LayoutSVGTransformableContainer_h
