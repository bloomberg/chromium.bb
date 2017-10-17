/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
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

#include "core/layout/svg/LayoutSVGResourceClipper.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/PaintInfo.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGUseElement.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

namespace blink {

namespace {

enum class ClipStrategy { kNone, kMask, kPath };

ClipStrategy ModifyStrategyForClipPath(const ComputedStyle& style,
                                       ClipStrategy strategy) {
  // If the shape in the clip-path gets clipped too then fallback to masking.
  if (strategy != ClipStrategy::kPath || !style.ClipPath())
    return strategy;
  return ClipStrategy::kMask;
}

ClipStrategy DetermineClipStrategy(const SVGGraphicsElement& element) {
  const LayoutObject* layout_object = element.GetLayoutObject();
  if (!layout_object)
    return ClipStrategy::kNone;
  const ComputedStyle& style = layout_object->StyleRef();
  if (style.Display() == EDisplay::kNone ||
      style.Visibility() != EVisibility::kVisible)
    return ClipStrategy::kNone;
  ClipStrategy strategy = ClipStrategy::kNone;
  // Only shapes, paths and texts are allowed for clipping.
  if (layout_object->IsSVGShape()) {
    strategy = ClipStrategy::kPath;
  } else if (layout_object->IsSVGText()) {
    // Text requires masking.
    strategy = ClipStrategy::kMask;
  }
  return ModifyStrategyForClipPath(style, strategy);
}

ClipStrategy DetermineClipStrategy(const SVGElement& element) {
  // <use> within <clipPath> have a restricted content model.
  // (https://drafts.fxtf.org/css-masking-1/#ClipPathElement)
  if (IsSVGUseElement(element)) {
    const LayoutObject* use_layout_object = element.GetLayoutObject();
    if (!use_layout_object ||
        use_layout_object->StyleRef().Display() == EDisplay::kNone)
      return ClipStrategy::kNone;
    const SVGGraphicsElement* shape_element =
        ToSVGUseElement(element).VisibleTargetGraphicsElementForClipping();
    if (!shape_element)
      return ClipStrategy::kNone;
    ClipStrategy shape_strategy = DetermineClipStrategy(*shape_element);
    return ModifyStrategyForClipPath(use_layout_object->StyleRef(),
                                     shape_strategy);
  }
  if (!element.IsSVGGraphicsElement())
    return ClipStrategy::kNone;
  return DetermineClipStrategy(ToSVGGraphicsElement(element));
}

bool ContributesToClip(const SVGElement& element) {
  return DetermineClipStrategy(element) != ClipStrategy::kNone;
}

void PathFromElement(const SVGElement& element, Path& clip_path) {
  if (auto* geometry = ToSVGGeometryElementOrNull(element))
    geometry->ToClipPath(clip_path);
  else if (auto* use = ToSVGUseElementOrNull(element))
    use->ToClipPath(clip_path);
}

}  // namespace

LayoutSVGResourceClipper::LayoutSVGResourceClipper(SVGClipPathElement* node)
    : LayoutSVGResourceContainer(node), in_clip_expansion_(false) {}

LayoutSVGResourceClipper::~LayoutSVGResourceClipper() {}

void LayoutSVGResourceClipper::RemoveAllClientsFromCache(
    bool mark_for_invalidation) {
  clip_content_path_.Clear();
  cached_paint_record_.reset();
  local_clip_bounds_ = FloatRect();
  MarkAllClientsForInvalidation(mark_for_invalidation
                                    ? kLayoutAndBoundariesInvalidation
                                    : kParentOnlyInvalidation);
}

void LayoutSVGResourceClipper::RemoveClientFromCache(
    LayoutObject* client,
    bool mark_for_invalidation) {
  DCHECK(client);
  MarkClientForInvalidation(client, mark_for_invalidation
                                        ? kBoundariesInvalidation
                                        : kParentOnlyInvalidation);
}

bool LayoutSVGResourceClipper::CalculateClipContentPathIfNeeded() {
  if (!clip_content_path_.IsEmpty())
    return true;

  // If the current clip-path gets clipped itself, we have to fallback to
  // masking.
  if (StyleRef().ClipPath())
    return false;

  unsigned op_count = 0;
  bool using_builder = false;
  SkOpBuilder clip_path_builder;

  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    ClipStrategy strategy = DetermineClipStrategy(child_element);
    if (strategy == ClipStrategy::kNone)
      continue;
    if (strategy == ClipStrategy::kMask) {
      clip_content_path_.Clear();
      return false;
    }

    // First clip shape.
    if (clip_content_path_.IsEmpty()) {
      PathFromElement(child_element, clip_content_path_);
      continue;
    }

    // Multiple shapes require PathOps. In some degenerate cases PathOps can
    // exhibit quadratic behavior, so we cap the number of ops to a reasonable
    // count.
    const unsigned kMaxOps = 42;
    if (++op_count > kMaxOps) {
      clip_content_path_.Clear();
      return false;
    }

    // Second clip shape => start using the builder.
    if (!using_builder) {
      clip_path_builder.add(clip_content_path_.GetSkPath(), kUnion_SkPathOp);
      using_builder = true;
    }

    Path sub_path;
    PathFromElement(child_element, sub_path);

    clip_path_builder.add(sub_path.GetSkPath(), kUnion_SkPathOp);
  }

  if (using_builder) {
    SkPath resolved_path;
    clip_path_builder.resolve(&resolved_path);
    clip_content_path_ = resolved_path;
  }

  return true;
}

bool LayoutSVGResourceClipper::AsPath(
    const AffineTransform& animated_local_transform,
    const FloatRect& reference_box,
    Path& clip_path) {
  if (!CalculateClipContentPathIfNeeded())
    return false;

  clip_path = clip_content_path_;

  // We are able to represent the clip as a path. Continue with direct clipping,
  // and transform the content to userspace if necessary.
  if (ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    AffineTransform transform;
    transform.Translate(reference_box.X(), reference_box.Y());
    transform.ScaleNonUniform(reference_box.Width(), reference_box.Height());
    clip_path.Transform(transform);
  }

  // Transform path by animatedLocalTransform.
  clip_path.Transform(animated_local_transform);
  return true;
}

sk_sp<const PaintRecord> LayoutSVGResourceClipper::CreatePaintRecord() {
  DCHECK(GetFrame());
  if (cached_paint_record_)
    return cached_paint_record_;

  // Using strokeBoundingBox (instead of visualRectInLocalSVGCoordinates) to
  // avoid the intersection with local clips/mask, which may yield incorrect
  // results when mixing objectBoundingBox and userSpaceOnUse units
  // (http://crbug.com/294900).
  FloatRect bounds = StrokeBoundingBox();

  PaintRecordBuilder builder(bounds, nullptr, nullptr);
  // Switch to a paint behavior where all children of this <clipPath> will be
  // laid out using special constraints:
  // - fill-opacity/stroke-opacity/opacity set to 1
  // - masker/filter not applied when laying out the children
  // - fill is set to the initial fill paint server (solid, black)
  // - stroke is set to the initial stroke paint server (none)
  PaintInfo info(builder.Context(), LayoutRect::InfiniteIntRect(),
                 PaintPhase::kForeground, kGlobalPaintNormalPhase,
                 kPaintLayerPaintingRenderingClipPathAsMask |
                     kPaintLayerPaintingRenderingResourceSubtree);

  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    // Use the LayoutObject of the direct child even if it is a <use>. In that
    // case, we will paint the targeted element indirectly.
    const LayoutObject* layout_object = child_element.GetLayoutObject();
    layout_object->Paint(info, IntPoint());
  }

  cached_paint_record_ = builder.EndRecording();
  return cached_paint_record_;
}

void LayoutSVGResourceClipper::CalculateLocalClipBounds() {
  // This is a rough heuristic to appraise the clip size and doesn't consider
  // clip on clip.
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    const LayoutObject* layout_object = child_element.GetLayoutObject();
    local_clip_bounds_.Unite(layout_object->LocalToSVGParentTransform().MapRect(
        layout_object->VisualRectInLocalSVGCoordinates()));
  }
}

bool LayoutSVGResourceClipper::HitTestClipContent(
    const FloatRect& object_bounding_box,
    const FloatPoint& node_at_point) {
  FloatPoint point = node_at_point;
  if (!SVGLayoutSupport::PointInClippingArea(*this, point))
    return false;

  if (ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    AffineTransform transform;
    transform.Translate(object_bounding_box.X(), object_bounding_box.Y());
    transform.ScaleNonUniform(object_bounding_box.Width(),
                              object_bounding_box.Height());
    point = transform.Inverse().MapPoint(point);
  }

  AffineTransform animated_local_transform =
      ToSVGClipPathElement(GetElement())
          ->CalculateTransform(SVGElement::kIncludeMotionTransform);
  if (!animated_local_transform.IsInvertible())
    return false;

  point = animated_local_transform.Inverse().MapPoint(point);

  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    IntPoint hit_point;
    HitTestResult result(HitTestRequest::kSVGClipContent, hit_point);
    LayoutObject* layout_object = child_element.GetLayoutObject();
    if (layout_object->NodeAtFloatPoint(result, point, kHitTestForeground))
      return true;
  }
  return false;
}

FloatRect LayoutSVGResourceClipper::ResourceBoundingBox(
    const FloatRect& reference_box) {
  // The resource has not been layouted yet. Return the reference box.
  if (SelfNeedsLayout())
    return reference_box;

  if (local_clip_bounds_.IsEmpty())
    CalculateLocalClipBounds();

  AffineTransform transform =
      ToSVGClipPathElement(GetElement())
          ->CalculateTransform(SVGElement::kIncludeMotionTransform);
  if (ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    transform.Translate(reference_box.X(), reference_box.Y());
    transform.ScaleNonUniform(reference_box.Width(), reference_box.Height());
  }
  return transform.MapRect(local_clip_bounds_);
}

}  // namespace blink
