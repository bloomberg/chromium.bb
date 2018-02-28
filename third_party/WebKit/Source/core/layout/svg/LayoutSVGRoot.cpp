/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "core/layout/svg/LayoutSVGRoot.h"

#include "core/frame/FrameOwner.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/LengthFunctions.h"

namespace blink {

LayoutSVGRoot::LayoutSVGRoot(SVGElement* node)
    : LayoutReplaced(node),
      object_bounding_box_valid_(false),
      is_layout_size_changed_(false),
      did_screen_scale_factor_change_(false),
      needs_boundaries_or_transform_update_(true),
      has_box_decoration_background_(false),
      has_non_isolated_blending_descendants_(false),
      has_non_isolated_blending_descendants_dirty_(false) {
  SVGSVGElement* svg = ToSVGSVGElement(node);
  DCHECK(svg);

  LayoutSize intrinsic_size(svg->IntrinsicWidth(), svg->IntrinsicHeight());
  if (!svg->HasIntrinsicWidth())
    intrinsic_size.SetWidth(LayoutUnit(kDefaultWidth));
  if (!svg->HasIntrinsicHeight())
    intrinsic_size.SetHeight(LayoutUnit(kDefaultHeight));
  SetIntrinsicSize(intrinsic_size);
}

LayoutSVGRoot::~LayoutSVGRoot() = default;

void LayoutSVGRoot::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  // https://www.w3.org/TR/SVG/coords.html#IntrinsicSizing

  SVGSVGElement* svg = ToSVGSVGElement(GetNode());
  DCHECK(svg);

  intrinsic_sizing_info.size =
      FloatSize(svg->IntrinsicWidth(), svg->IntrinsicHeight());
  intrinsic_sizing_info.has_width = svg->HasIntrinsicWidth();
  intrinsic_sizing_info.has_height = svg->HasIntrinsicHeight();

  if (!intrinsic_sizing_info.size.IsEmpty()) {
    intrinsic_sizing_info.aspect_ratio = intrinsic_sizing_info.size;
  } else {
    FloatSize view_box_size = svg->viewBox()->CurrentValue()->Value().Size();
    if (!view_box_size.IsEmpty()) {
      // The viewBox can only yield an intrinsic ratio, not an intrinsic size.
      intrinsic_sizing_info.aspect_ratio = view_box_size;
    }
  }

  if (!IsHorizontalWritingMode())
    intrinsic_sizing_info.Transpose();
}

bool LayoutSVGRoot::IsEmbeddedThroughSVGImage() const {
  return SVGImage::IsInSVGImage(ToSVGSVGElement(GetNode()));
}

bool LayoutSVGRoot::IsEmbeddedThroughFrameContainingSVGDocument() const {
  if (!GetNode())
    return false;

  LocalFrame* frame = GetNode()->GetDocument().GetFrame();
  if (!frame || !frame->GetDocument()->IsSVGDocument())
    return false;

  if (frame->Owner() && frame->Owner()->IsRemote())
    return true;

  // If our frame has an owner layoutObject, we're embedded through eg.
  // object/embed/iframe, but we only negotiate if we're in an SVG document
  // inside a embedded object (object/embed).
  LayoutObject* owner_layout_object = frame->OwnerLayoutObject();
  return owner_layout_object && owner_layout_object->IsEmbeddedObject();
}

LayoutUnit LayoutSVGRoot::ComputeReplacedLogicalWidth(
    ShouldComputePreferred should_compute_preferred) const {
  // When we're embedded through SVGImage
  // (border-image/background-image/<html:img>/...) we're forced to resize to a
  // specific size.
  if (!container_size_.IsEmpty())
    return LayoutUnit(container_size_.Width());

  if (IsEmbeddedThroughFrameContainingSVGDocument())
    return ContainingBlock()->AvailableLogicalWidth();

  return LayoutReplaced::ComputeReplacedLogicalWidth(should_compute_preferred);
}

LayoutUnit LayoutSVGRoot::ComputeReplacedLogicalHeight(
    LayoutUnit estimated_used_width) const {
  // When we're embedded through SVGImage
  // (border-image/background-image/<html:img>/...) we're forced to resize to a
  // specific size.
  if (!container_size_.IsEmpty())
    return LayoutUnit(container_size_.Height());

  if (IsEmbeddedThroughFrameContainingSVGDocument())
    return ContainingBlock()->AvailableLogicalHeight(
        kIncludeMarginBorderPadding);

  const Length& logical_height = Style()->LogicalHeight();
  if (IsDocumentElement() && logical_height.IsPercentOrCalc()) {
    return ValueForLength(
        logical_height,
        GetDocument().GetLayoutView()->ViewLogicalHeightForPercentages());
  }

  return LayoutReplaced::ComputeReplacedLogicalHeight(estimated_used_width);
}

void LayoutSVGRoot::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  LayoutSize old_size = Size();
  UpdateLogicalWidth();
  UpdateLogicalHeight();

  // The local-to-border-box transform is a function with the following as
  // input:
  //
  //  * effective zoom
  //  * contentWidth/Height
  //  * viewBox
  //  * border + padding
  //  * currentTranslate
  //  * currentScale
  //
  // Which means that |transformChange| will notice a change to the scale from
  // any of these.
  SVGTransformChange transform_change = BuildLocalToBorderBoxTransform();

  // The scale factor from the local-to-border-box transform is all that our
  // scale-dependent descendants care about.
  did_screen_scale_factor_change_ =
      transform_change == SVGTransformChange::kFull;

  SVGLayoutSupport::LayoutResourcesIfNeeded(*this);

  // selfNeedsLayout() will cover changes to one (or more) of viewBox,
  // current{Scale,Translate}, decorations and 'overflow'.
  const bool viewport_may_have_changed =
      SelfNeedsLayout() || old_size != Size();

  // The scale of one or more of the SVG elements may have changed, content
  // (the entire SVG) could have moved or new content may have been exposed, so
  // mark the entire subtree as needing paint invalidation checking.
  if (transform_change != SVGTransformChange::kNone ||
      viewport_may_have_changed) {
    SetMayNeedPaintInvalidationSubtree();
    SetNeedsPaintPropertyUpdate();
  }

  SVGSVGElement* svg = ToSVGSVGElement(GetNode());
  DCHECK(svg);
  // When hasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  is_layout_size_changed_ =
      viewport_may_have_changed && svg->HasRelativeLengths();

  SVGLayoutSupport::LayoutChildren(FirstChild(), false,
                                   did_screen_scale_factor_change_,
                                   is_layout_size_changed_);

  if (needs_boundaries_or_transform_update_) {
    UpdateCachedBoundaries();
    needs_boundaries_or_transform_update_ = false;
  }

  overflow_.reset();
  AddVisualEffectOverflow();

  if (!ShouldApplyViewportClip()) {
    FloatRect content_visual_rect = VisualRectInLocalSVGCoordinates();
    content_visual_rect =
        local_to_border_box_transform_.MapRect(content_visual_rect);
    AddContentsVisualOverflow(EnclosingLayoutRect(content_visual_rect));
  }

  UpdateAfterLayout();
  has_box_decoration_background_ = IsDocumentElement()
                                       ? StyleRef().HasBoxDecorationBackground()
                                       : HasBoxDecorationBackground();
  InvalidateBackgroundObscurationStatus();

  ClearNeedsLayout();
}

bool LayoutSVGRoot::ShouldApplyViewportClip() const {
  // the outermost svg is clipped if auto, and svg document roots are always
  // clipped. When the svg is stand-alone (isDocumentElement() == true) the
  // viewport clipping should always be applied, noting that the window
  // scrollbars should be hidden if overflow=hidden.
  return Style()->OverflowX() == EOverflow::kHidden ||
         Style()->OverflowX() == EOverflow::kAuto ||
         Style()->OverflowX() == EOverflow::kScroll || IsDocumentElement();
}

LayoutRect LayoutSVGRoot::VisualOverflowRect() const {
  LayoutRect rect = LayoutReplaced::SelfVisualOverflowRect();
  if (!ShouldApplyViewportClip())
    rect.Unite(ContentsVisualOverflowRect());
  return rect;
}

void LayoutSVGRoot::PaintReplaced(const PaintInfo& paint_info,
                                  const LayoutPoint& paint_offset) const {
  SVGRootPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutSVGRoot::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(*this);
  LayoutReplaced::WillBeDestroyed();
}

bool LayoutSVGRoot::IntrinsicSizeIsFontMetricsDependent() const {
  const SVGSVGElement& svg = ToSVGSVGElement(*GetNode());
  return svg.width()->CurrentValue()->IsFontRelative() ||
         svg.height()->CurrentValue()->IsFontRelative();
}

bool LayoutSVGRoot::StyleChangeAffectsIntrinsicSize(
    const ComputedStyle& old_style) const {
  const ComputedStyle& style = StyleRef();
  // If the writing mode changed from a horizontal mode to a vertical
  // mode, or vice versa, then our intrinsic dimensions will have
  // changed.
  if (old_style.IsHorizontalWritingMode() != style.IsHorizontalWritingMode())
    return true;
  // If our intrinsic dimensions depend on font metrics (by using 'em', 'ex' or
  // any other font-relative unit), any changes to the font may change said
  // dimensions.
  if (IntrinsicSizeIsFontMetricsDependent() &&
      old_style.GetFont() != style.GetFont())
    return true;
  return false;
}

void LayoutSVGRoot::IntrinsicSizingInfoChanged() const {
  // TODO(fs): Merge with IntrinsicSizeChanged()? (from LayoutReplaced)
  // Ignore changes to intrinsic dimensions if the <svg> is not in an SVG
  // document, or not embedded in a way that supports/allows size negotiation.
  if (!IsEmbeddedThroughFrameContainingSVGDocument())
    return;
  IntrinsicSizingInfo sizing_info;
  ComputeIntrinsicSizingInfo(sizing_info);
  GetFrame()->IntrinsicSizingInfoChanged(sizing_info);
}

void LayoutSVGRoot::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  if (diff.NeedsFullLayout())
    SetNeedsBoundariesUpdate();
  if (diff.NeedsFullPaintInvalidation()) {
    // Box decorations may have appeared/disappeared - recompute status.
    has_box_decoration_background_ = StyleRef().HasBoxDecorationBackground();
  }

  // If we previously didn't have any computed style, we wouldn't have been
  // able to determine our intrinsic dimensions, so in that case always
  // initiate a size negotiation.
  if (!old_style || StyleChangeAffectsIntrinsicSize(*old_style))
    IntrinsicSizingInfoChanged();

  LayoutReplaced::StyleDidChange(diff, old_style);
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

bool LayoutSVGRoot::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  return child->IsSVG() && !(child->IsSVGInline() || child->IsSVGInlineText());
}

void LayoutSVGRoot::AddChild(LayoutObject* child, LayoutObject* before_child) {
  LayoutReplaced::AddChild(child, before_child);
  SVGResourcesCache::ClientWasAddedToTree(*child, child->StyleRef());

  bool should_isolate_descendants =
      (child->IsBlendingAllowed() && child->Style()->HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (should_isolate_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationRequired);
}

void LayoutSVGRoot::RemoveChild(LayoutObject* child) {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*child);
  LayoutReplaced::RemoveChild(child);

  bool had_non_isolated_descendants =
      (child->IsBlendingAllowed() && child->Style()->HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (had_non_isolated_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationNeedsUpdate);
}

bool LayoutSVGRoot::HasNonIsolatedBlendingDescendants() const {
  if (has_non_isolated_blending_descendants_dirty_) {
    has_non_isolated_blending_descendants_ =
        SVGLayoutSupport::ComputeHasNonIsolatedBlendingDescendants(this);
    has_non_isolated_blending_descendants_dirty_ = false;
  }
  return has_non_isolated_blending_descendants_;
}

void LayoutSVGRoot::DescendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
  switch (state) {
    case kDescendantIsolationRequired:
      has_non_isolated_blending_descendants_ = true;
      has_non_isolated_blending_descendants_dirty_ = false;
      break;
    case kDescendantIsolationNeedsUpdate:
      has_non_isolated_blending_descendants_dirty_ = true;
      break;
  }
  SetNeedsPaintPropertyUpdate();
}

void LayoutSVGRoot::InsertedIntoTree() {
  LayoutReplaced::InsertedIntoTree();
  SVGResourcesCache::ClientWasAddedToTree(*this, StyleRef());
}

void LayoutSVGRoot::WillBeRemovedFromTree() {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*this);
  LayoutReplaced::WillBeRemovedFromTree();
}

PositionWithAffinity LayoutSVGRoot::PositionForPoint(
    const LayoutPoint& point) const {
  FloatPoint absolute_point = FloatPoint(point);
  absolute_point =
      local_to_border_box_transform_.Inverse().MapPoint(absolute_point);
  LayoutObject* closest_descendant =
      SVGLayoutSupport::FindClosestLayoutSVGText(this, absolute_point);

  if (!closest_descendant)
    return LayoutReplaced::PositionForPoint(point);

  LayoutObject* layout_object = closest_descendant;
  AffineTransform transform = layout_object->LocalToSVGParentTransform();
  transform.Translate(ToLayoutSVGText(layout_object)->Location().X(),
                      ToLayoutSVGText(layout_object)->Location().Y());
  while (layout_object) {
    layout_object = layout_object->Parent();
    if (layout_object->IsSVGRoot())
      break;
    transform = layout_object->LocalToSVGParentTransform() * transform;
  }

  absolute_point = transform.Inverse().MapPoint(absolute_point);

  return closest_descendant->PositionForPoint(LayoutPoint(absolute_point));
}

// LayoutBox methods will expect coordinates w/o any transforms in coordinates
// relative to our borderBox origin.  This method gives us exactly that.
SVGTransformChange LayoutSVGRoot::BuildLocalToBorderBoxTransform() {
  SVGTransformChangeDetector change_detector(local_to_border_box_transform_);
  SVGSVGElement* svg = ToSVGSVGElement(GetNode());
  DCHECK(svg);
  float scale = Style()->EffectiveZoom();
  local_to_border_box_transform_ = svg->ViewBoxToViewTransform(
      ContentWidth() / scale, ContentHeight() / scale);

  FloatPoint translate = svg->CurrentTranslate();
  LayoutSize border_and_padding(BorderLeft() + PaddingLeft(),
                                BorderTop() + PaddingTop());
  AffineTransform view_to_border_box_transform(
      scale, 0, 0, scale, border_and_padding.Width() + translate.X(),
      border_and_padding.Height() + translate.Y());
  view_to_border_box_transform.Scale(svg->currentScale());
  local_to_border_box_transform_.PreMultiply(view_to_border_box_transform);
  return change_detector.ComputeChange(local_to_border_box_transform_);
}

AffineTransform LayoutSVGRoot::LocalToSVGParentTransform() const {
  return AffineTransform::Translation(RoundToInt(Location().X()),
                                      RoundToInt(Location().Y())) *
         local_to_border_box_transform_;
}

LayoutRect LayoutSVGRoot::LocalVisualRectIgnoringVisibility() const {
  // This is an open-coded aggregate of SVGLayoutSupport::localVisualRect
  // and LayoutReplaced::localVisualRect. The reason for this is to optimize/
  // minimize the visual rect when the box is not "decorated" (does not have
  // background/border/etc., see
  // LayoutSVGRootTest.VisualRectMappingWithViewportClipWithoutBorder).

  // Return early for any cases where we don't actually paint.
  if (!EnclosingLayer()->HasVisibleContent())
    return LayoutRect();

  // Compute the visual rect of the content of the SVG in the border-box
  // coordinate space.
  FloatRect content_visual_rect = VisualRectInLocalSVGCoordinates();
  content_visual_rect =
      local_to_border_box_transform_.MapRect(content_visual_rect);

  // Apply initial viewport clip, overflow:visible content is added to
  // visualOverflow but the most common case is that overflow is hidden, so
  // always intersect.
  content_visual_rect.Intersect(PixelSnappedBorderBoxRect());

  LayoutRect visual_rect = EnclosingLayoutRect(content_visual_rect);
  // If the box is decorated or is overflowing, extend it to include the
  // border-box and overflow.
  if (has_box_decoration_background_ || HasOverflowModel()) {
    // The selectionRect can project outside of the overflowRect, so take their
    // union for paint invalidation to avoid selection painting glitches.
    LayoutRect decorated_visual_rect =
        UnionRect(LocalSelectionRect(), VisualOverflowRect());
    visual_rect.Unite(decorated_visual_rect);
  }

  return LayoutRect(EnclosingIntRect(visual_rect));
}

bool LayoutSVGRoot::PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
  // The rule extends LayoutBox's instead of LayoutReplaced's.
  if (!LayoutBox::PaintedOutputOfObjectHasNoEffectRegardlessOfSize())
    return false;

  if (SVGResources* resources =
          SVGResourcesCache::CachedResourcesForLayoutObject(*this)) {
    if (resources->Masker())
      return false;
  }
  return true;
}

// This method expects local CSS box coordinates.
// Callers with local SVG viewport coordinates should first apply the
// localToBorderBoxTransform to convert from SVG viewport coordinates to local
// CSS box coordinates.
void LayoutSVGRoot::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                       TransformState& transform_state,
                                       MapCoordinatesFlags mode) const {
  LayoutReplaced::MapLocalToAncestor(ancestor, transform_state,
                                     mode | kApplyContainerFlip);
}

const LayoutObject* LayoutSVGRoot::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  return LayoutReplaced::PushMappingToContainer(ancestor_to_stop_at,
                                                geometry_map);
}

void LayoutSVGRoot::UpdateCachedBoundaries() {
  SVGLayoutSupport::ComputeContainerBoundingBoxes(
      this, object_bounding_box_, object_bounding_box_valid_,
      stroke_bounding_box_, visual_rect_in_local_svg_coordinates_);
}

bool LayoutSVGRoot::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& location_in_container,
                                const LayoutPoint& accumulated_offset,
                                HitTestAction hit_test_action) {
  LayoutPoint point_in_parent =
      location_in_container.Point() - ToLayoutSize(accumulated_offset);
  LayoutPoint point_in_border_box = point_in_parent - ToLayoutSize(Location());

  // Only test SVG content if the point is in our content box, or in case we
  // don't clip to the viewport, the visual overflow rect.
  // FIXME: This should be an intersection when rect-based hit tests are
  // supported by nodeAtFloatPoint.
  if (ContentBoxRect().Contains(point_in_border_box) ||
      (!ShouldApplyViewportClip() &&
       VisualOverflowRect().Contains(point_in_border_box))) {
    const AffineTransform& local_to_parent_transform =
        LocalToSVGParentTransform();
    if (local_to_parent_transform.IsInvertible()) {
      FloatPoint local_point = local_to_parent_transform.Inverse().MapPoint(
          FloatPoint(point_in_parent));

      for (LayoutObject* child = LastChild(); child;
           child = child->PreviousSibling()) {
        // FIXME: nodeAtFloatPoint() doesn't handle rect-based hit tests yet.
        if (child->NodeAtFloatPoint(result, local_point, hit_test_action)) {
          UpdateHitTestResult(result, point_in_border_box);
          if (result.AddNodeToListBasedTestResult(
                  child->GetNode(), location_in_container) == kStopHitTesting)
            return true;
        }
      }
    }
  }

  // If we didn't early exit above, we've just hit the container <svg> element.
  // Unlike SVG 1.1, 2nd Edition allows container elements to be hit.
  if ((hit_test_action == kHitTestBlockBackground ||
       hit_test_action == kHitTestChildBlockBackground) &&
      VisibleToHitTestRequest(result.GetHitTestRequest())) {
    // Only return true here, if the last hit testing phase 'BlockBackground'
    // (or 'ChildBlockBackground' - depending on context) is executed.
    // If we'd return true in the 'Foreground' phase, hit testing would stop
    // immediately. For SVG only trees this doesn't matter.
    // Though when we have a <foreignObject> subtree we need to be able to
    // detect hits on the background of a <div> element.
    // If we'd return true here in the 'Foreground' phase, we are not able to
    // detect these hits anymore.
    LayoutRect bounds_rect(accumulated_offset + Location(), Size());
    if (location_in_container.Intersects(bounds_rect)) {
      UpdateHitTestResult(result, point_in_border_box);
      if (result.AddNodeToListBasedTestResult(GetNode(), location_in_container,
                                              bounds_rect) == kStopHitTesting)
        return true;
    }
  }

  return false;
}

}  // namespace blink
