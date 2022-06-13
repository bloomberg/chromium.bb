/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"

#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

LayoutSVGBlock::LayoutSVGBlock(Element* element)
    : LayoutBlockFlow(element),
      needs_transform_update_(true),
      transform_uses_reference_box_(false) {
  DCHECK(IsA<SVGElement>(element));
}

SVGElement* LayoutSVGBlock::GetElement() const {
  NOT_DESTROYED();
  return To<SVGElement>(LayoutObject::GetNode());
}

void LayoutSVGBlock::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearClipPathFilterMask(*GetElement(), Style());
  LayoutBlockFlow::WillBeDestroyed();
}

void LayoutSVGBlock::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutBlockFlow::InsertedIntoTree();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
  if (CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(*this) !=
      CompositingReason::kNone) {
    SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(this);
  }
}

void LayoutSVGBlock::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);
  if (StyleRef().HasSVGEffect())
    SetNeedsPaintPropertyUpdate();
  LayoutBlockFlow::WillBeRemovedFromTree();
  if (CompositingReasonFinder::DirectReasonsForSVGChildPaintProperties(*this) !=
      CompositingReason::kNone) {
    SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(this);
  }
}

void LayoutSVGBlock::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutBlockFlow::UpdateFromStyle();
  SetFloating(false);
}

bool LayoutSVGBlock::CheckForImplicitTransformChange(bool bbox_changed) const {
  NOT_DESTROYED();
  // If the transform is relative to the reference box, check relevant
  // conditions to see if we need to recompute the transform.
  switch (StyleRef().TransformBox()) {
    case ETransformBox::kViewBox:
      return SVGLayoutSupport::LayoutSizeOfNearestViewportChanged(this);
    case ETransformBox::kFillBox:
      return bbox_changed;
  }
  NOTREACHED();
  return false;
}

bool LayoutSVGBlock::UpdateTransformAfterLayout(bool bounds_changed) {
  NOT_DESTROYED();
  // If our transform depends on the reference box, we need to check if it needs
  // to be updated.
  if (!needs_transform_update_ && transform_uses_reference_box_) {
    needs_transform_update_ = CheckForImplicitTransformChange(bounds_changed);
    if (needs_transform_update_)
      SetNeedsPaintPropertyUpdate();
  }
  if (!needs_transform_update_)
    return false;
  local_transform_ =
      GetElement()->CalculateTransform(SVGElement::kIncludeMotionTransform);
  needs_transform_update_ = false;
  return true;
}

void LayoutSVGBlock::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlock::StyleDidChange(diff, old_style);

  // |HasTransformRelatedProperty| is used for compositing so ensure it was
  // correctly set by the call to |StyleDidChange|.
  DCHECK_EQ(HasTransformRelatedProperty(),
            StyleRef().HasTransformRelatedProperty());

  transform_uses_reference_box_ =
      TransformHelper::DependsOnReferenceBox(StyleRef());

  if (diff.NeedsFullLayout()) {
    SetNeedsBoundariesUpdate();
    if (diff.TransformChanged())
      SetNeedsTransformUpdate();
  }

  SVGResources::UpdateClipPathFilterMask(*GetElement(), old_style, StyleRef());

  if (!Parent())
    return;
  if (diff.BlendModeChanged()) {
    DCHECK(IsBlendingAllowed());
    Parent()->DescendantIsolationRequirementsChanged(
        StyleRef().HasBlendMode() ? kDescendantIsolationRequired
                                  : kDescendantIsolationNeedsUpdate);
  }
  if (diff.CompositingReasonsChanged())
    SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(this);
  if (diff.HasDifference())
    LayoutSVGResourceContainer::StyleChanged(*this, diff);
}

void LayoutSVGBlock::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                        TransformState& transform_state,
                                        MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  // Convert from local HTML coordinates to local SVG coordinates.
  transform_state.Move(PhysicalLocation());
  // Apply other mappings on local SVG coordinates.
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

void LayoutSVGBlock::MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                        TransformState& transform_state,
                                        MapCoordinatesFlags flags) const {
  NOT_DESTROYED();
  if (this == ancestor)
    return;

  // Map to local SVG coordinates.
  SVGLayoutSupport::MapAncestorToLocal(*this, ancestor, transform_state, flags);
  // Convert from local SVG coordinates to local HTML coordinates.
  transform_state.Move(PhysicalLocation());
}

PhysicalRect LayoutSVGBlock::VisualRectInDocument(VisualRectFlags flags) const {
  NOT_DESTROYED();
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View(), flags);
}

bool LayoutSVGBlock::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags) const {
  NOT_DESTROYED();
  transform_state.Flatten();
  PhysicalRect rect(LayoutRect(transform_state.LastPlanarQuad().BoundingBox()));
  // Convert from local HTML coordinates to local SVG coordinates.
  rect.Move(PhysicalLocation());
  // Apply other mappings on local SVG coordinates.
  bool retval = SVGLayoutSupport::MapToVisualRectInAncestorSpace(
      *this, ancestor, gfx::RectF(rect), rect);
  transform_state.SetQuad(FloatQuad(FloatRect(rect)));
  return retval;
}

bool LayoutSVGBlock::NodeAtPoint(HitTestResult&,
                                 const HitTestLocation&,
                                 const PhysicalOffset&,
                                 HitTestAction) {
  NOT_DESTROYED();
  NOTREACHED();
  return false;
}

}  // namespace blink
