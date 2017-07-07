/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/layout/svg/LayoutSVGTransformableContainer.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/svg/SVGGElement.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGUseElement.h"

namespace blink {

LayoutSVGTransformableContainer::LayoutSVGTransformableContainer(
    SVGGraphicsElement* node)
    : LayoutSVGContainer(node), needs_transform_update_(true) {}

static bool HasValidPredecessor(const Node* node) {
  DCHECK(node);
  for (node = node->previousSibling(); node; node = node->previousSibling()) {
    if (node->IsSVGElement() && ToSVGElement(node)->IsValid())
      return true;
  }
  return false;
}

bool LayoutSVGTransformableContainer::IsChildAllowed(
    LayoutObject* child,
    const ComputedStyle& style) const {
  DCHECK(GetElement());
  if (isSVGSwitchElement(*GetElement())) {
    Node* node = child->GetNode();
    // Reject non-SVG/non-valid elements.
    if (!node->IsSVGElement() || !ToSVGElement(node)->IsValid())
      return false;
    // Reject this child if it isn't the first valid node.
    if (HasValidPredecessor(node))
      return false;
  } else if (isSVGAElement(*GetElement())) {
    // http://www.w3.org/2003/01/REC-SVG11-20030114-errata#linking-text-environment
    // The 'a' element may contain any element that its parent may contain,
    // except itself.
    if (isSVGAElement(*child->GetNode()))
      return false;
    if (Parent() && Parent()->IsSVG())
      return Parent()->IsChildAllowed(child, style);
  }
  return LayoutSVGContainer::IsChildAllowed(child, style);
}

void LayoutSVGTransformableContainer::SetNeedsTransformUpdate() {
  SetMayNeedPaintInvalidationSubtree();
  // The transform paint property relies on the SVG transform being up-to-date
  // (see: PaintPropertyTreeBuilder::updateTransformForNonRootSVG).
  SetNeedsPaintPropertyUpdate();
  needs_transform_update_ = true;
}

SVGTransformChange LayoutSVGTransformableContainer::CalculateLocalTransform() {
  SVGGraphicsElement* element = ToSVGGraphicsElement(this->GetElement());
  DCHECK(element);

  // If we're either the layoutObject for a <use> element, or for any <g>
  // element inside the shadow tree, that was created during the use/symbol/svg
  // expansion in SVGUseElement. These containers need to respect the
  // translations induced by their corresponding use elements x/y attributes.
  SVGUseElement* use_element = nullptr;
  if (isSVGUseElement(*element)) {
    use_element = toSVGUseElement(element);
  } else if (isSVGGElement(*element) &&
             toSVGGElement(element)->InUseShadowTree()) {
    SVGElement* corresponding_element = element->CorrespondingElement();
    if (isSVGUseElement(corresponding_element))
      use_element = toSVGUseElement(corresponding_element);
  }

  if (use_element) {
    SVGLengthContext length_context(element);
    FloatSize translation(
        use_element->x()->CurrentValue()->Value(length_context),
        use_element->y()->CurrentValue()->Value(length_context));
    // TODO(fs): Signal this on style update instead. (Since these are
    // suppose to be presentation attributes now, this does feel a bit
    // broken...)
    if (translation != additional_translation_)
      SetNeedsTransformUpdate();
    additional_translation_ = translation;
  }

  if (!needs_transform_update_)
    return SVGTransformChange::kNone;

  SVGTransformChangeDetector change_detector(local_transform_);
  local_transform_ =
      element->CalculateTransform(SVGElement::kIncludeMotionTransform);
  local_transform_.Translate(additional_translation_.Width(),
                             additional_translation_.Height());
  needs_transform_update_ = false;
  return change_detector.ComputeChange(local_transform_);
}

}  // namespace blink
