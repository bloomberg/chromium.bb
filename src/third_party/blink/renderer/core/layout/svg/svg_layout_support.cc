/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"

#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_transformable_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

struct SearchCandidate {
  SearchCandidate()
      : layout_object(nullptr), distance(std::numeric_limits<float>::max()) {}
  SearchCandidate(LayoutObject* layout_object, float distance)
      : layout_object(layout_object), distance(distance) {}
  LayoutObject* layout_object;
  float distance;
};

FloatRect SVGLayoutSupport::LocalVisualRect(const LayoutObject& object) {
  // For LayoutSVGRoot, use LayoutSVGRoot::localVisualRect() instead.
  DCHECK(!object.IsSVGRoot());

  // Return early for any cases where we don't actually paint
  if (object.StyleRef().Visibility() != EVisibility::kVisible &&
      !object.EnclosingLayer()->HasVisibleContent())
    return FloatRect();

  FloatRect visual_rect = object.VisualRectInLocalSVGCoordinates();
  if (int outline_outset = object.StyleRef().OutlineOutsetExtent())
    visual_rect.Inflate(outline_outset);
  return visual_rect;
}

PhysicalRect SVGLayoutSupport::VisualRectInAncestorSpace(
    const LayoutObject& object,
    const LayoutBoxModelObject& ancestor,
    VisualRectFlags flags) {
  PhysicalRect rect;
  MapToVisualRectInAncestorSpace(object, &ancestor, LocalVisualRect(object),
                                 rect, flags);
  return rect;
}

static FloatRect MapToSVGRootIncludingFilter(
    const LayoutObject& object,
    const FloatRect& local_visual_rect) {
  DCHECK(object.IsSVGChild());

  FloatRect visual_rect = local_visual_rect;
  const LayoutObject* parent = &object;
  for (; !parent->IsSVGRoot(); parent = parent->Parent()) {
    const ComputedStyle& style = parent->StyleRef();
    if (style.HasFilter())
      visual_rect = style.Filter().MapRect(visual_rect);
    visual_rect = parent->LocalToSVGParentTransform().MapRect(visual_rect);
  }

  return To<LayoutSVGRoot>(*parent).LocalToBorderBoxTransform().MapRect(
      visual_rect);
}

static const LayoutSVGRoot& ComputeTransformToSVGRoot(
    const LayoutObject& object,
    AffineTransform& root_border_box_transform,
    bool* filter_skipped) {
  DCHECK(object.IsSVGChild());

  const LayoutObject* parent = &object;
  for (; !parent->IsSVGRoot(); parent = parent->Parent()) {
    if (filter_skipped && parent->StyleRef().HasFilter())
      *filter_skipped = true;
    root_border_box_transform.PreMultiply(parent->LocalToSVGParentTransform());
  }

  const auto& svg_root = To<LayoutSVGRoot>(*parent);
  root_border_box_transform.PreMultiply(svg_root.LocalToBorderBoxTransform());
  return svg_root;
}

bool SVGLayoutSupport::MapToVisualRectInAncestorSpace(
    const LayoutObject& object,
    const LayoutBoxModelObject* ancestor,
    const FloatRect& local_visual_rect,
    PhysicalRect& result_rect,
    VisualRectFlags visual_rect_flags) {
  AffineTransform root_border_box_transform;
  bool filter_skipped = false;
  const LayoutSVGRoot& svg_root = ComputeTransformToSVGRoot(
      object, root_border_box_transform, &filter_skipped);

  FloatRect adjusted_rect;
  if (filter_skipped)
    adjusted_rect = MapToSVGRootIncludingFilter(object, local_visual_rect);
  else
    adjusted_rect = root_border_box_transform.MapRect(local_visual_rect);

  if (adjusted_rect.IsEmpty()) {
    result_rect = PhysicalRect();
  } else {
    // Use EnclosingIntRect because we cannot properly apply subpixel offset of
    // the SVGRoot since we don't know the desired subpixel accumulation at this
    // point.
    result_rect = PhysicalRect(EnclosingIntRect(adjusted_rect));
  }

  // Apply initial viewport clip.
  if (svg_root.ShouldApplyViewportClip()) {
    PhysicalRect clip_rect(svg_root.OverflowClipRect(PhysicalOffset()));
    if (visual_rect_flags & kEdgeInclusive) {
      if (!result_rect.InclusiveIntersect(clip_rect))
        return false;
    } else {
      result_rect.Intersect(clip_rect);
    }
  }
  return svg_root.MapToVisualRectInAncestorSpace(ancestor, result_rect,
                                                 visual_rect_flags);
}

void SVGLayoutSupport::MapLocalToAncestor(const LayoutObject* object,
                                          const LayoutBoxModelObject* ancestor,
                                          TransformState& transform_state,
                                          MapCoordinatesFlags flags) {
  transform_state.ApplyTransform(object->LocalToSVGParentTransform());

  LayoutObject* parent = object->Parent();

  // At the SVG/HTML boundary (aka LayoutSVGRoot), we apply the
  // localToBorderBoxTransform to map an element from SVG viewport coordinates
  // to CSS box coordinates.
  // LayoutSVGRoot's mapLocalToAncestor method expects CSS box coordinates.
  if (parent->IsSVGRoot()) {
    transform_state.ApplyTransform(
        To<LayoutSVGRoot>(parent)->LocalToBorderBoxTransform());
  }

  parent->MapLocalToAncestor(ancestor, transform_state, flags);
}

void SVGLayoutSupport::MapAncestorToLocal(const LayoutObject& object,
                                          const LayoutBoxModelObject* ancestor,
                                          TransformState& transform_state,
                                          MapCoordinatesFlags flags) {
  // |object| is either a LayoutSVGModelObject or a LayoutSVGBlock here. In
  // the former case, |object| can never be an ancestor while in the latter
  // the caller is responsible for doing the ancestor check. Because of this,
  // computing the transform to the SVG root is always what we want to do here.
  DCHECK_NE(ancestor, &object);
  DCHECK(object.IsSVGContainer() || object.IsSVGShape() ||
         object.IsSVGImage() || object.IsSVGText() ||
         object.IsSVGForeignObject());
  AffineTransform local_to_svg_root;
  const LayoutSVGRoot& svg_root =
      ComputeTransformToSVGRoot(object, local_to_svg_root, nullptr);

  svg_root.MapAncestorToLocal(ancestor, transform_state, flags);

  transform_state.ApplyTransform(local_to_svg_root);
}

bool SVGLayoutSupport::LayoutSizeOfNearestViewportChanged(
    const LayoutObject* start) {
  for (; start; start = start->Parent()) {
    if (start->IsSVGRoot())
      return To<LayoutSVGRoot>(start)->IsLayoutSizeChanged();
    if (start->IsSVGViewportContainer())
      return To<LayoutSVGViewportContainer>(start)->IsLayoutSizeChanged();
  }
  NOTREACHED();
  return false;
}

bool SVGLayoutSupport::ScreenScaleFactorChanged(const LayoutObject* ancestor) {
  for (; ancestor; ancestor = ancestor->Parent()) {
    if (ancestor->IsSVGRoot())
      return To<LayoutSVGRoot>(ancestor)->DidScreenScaleFactorChange();
    if (ancestor->IsSVGTransformableContainer())
      return To<LayoutSVGTransformableContainer>(ancestor)
          ->DidScreenScaleFactorChange();
    if (ancestor->IsSVGViewportContainer())
      return To<LayoutSVGViewportContainer>(ancestor)
          ->DidScreenScaleFactorChange();
  }
  NOTREACHED();
  return false;
}

bool SVGLayoutSupport::IsOverflowHidden(const LayoutObject& object) {
  // LayoutSVGRoot should never query for overflow state - it should always clip
  // itself to the initial viewport size.
  DCHECK(!object.IsDocumentElement());
  return IsOverflowHidden(object.StyleRef());
}

bool SVGLayoutSupport::IsOverflowHidden(const ComputedStyle& style) {
  return style.OverflowX() == EOverflow::kHidden ||
         style.OverflowX() == EOverflow::kScroll;
}

void SVGLayoutSupport::AdjustWithClipPathAndMask(
    const LayoutObject& layout_object,
    const FloatRect& object_bounding_box,
    FloatRect& visual_rect) {
  SVGResourceClient* client = SVGResources::GetClient(layout_object);
  if (!client)
    return;
  const ComputedStyle& style = layout_object.StyleRef();
  if (LayoutSVGResourceClipper* clipper =
          GetSVGResourceAsType(*client, style.ClipPath()))
    visual_rect.Intersect(clipper->ResourceBoundingBox(object_bounding_box));
  if (auto* masker = GetSVGResourceAsType<LayoutSVGResourceMasker>(
          *client, style.MaskerResource()))
    visual_rect.Intersect(masker->ResourceBoundingBox(object_bounding_box, 1));
}

FloatRect SVGLayoutSupport::ExtendTextBBoxWithStroke(
    const LayoutObject& layout_object,
    const FloatRect& text_bounds) {
  DCHECK(layout_object.IsSVGText() || layout_object.IsNGSVGText() ||
         layout_object.IsSVGInline());
  FloatRect bounds = text_bounds;
  const ComputedStyle& style = layout_object.StyleRef();
  if (style.HasStroke()) {
    SVGLengthContext length_context(To<SVGElement>(layout_object.GetNode()));
    // TODO(fs): This approximation doesn't appear to be conservative enough
    // since while text (usually?) won't have caps it could have joins and thus
    // miters.
    bounds.Inflate(length_context.ValueForLength(style.StrokeWidth()));
  }
  return bounds;
}

FloatRect SVGLayoutSupport::ComputeVisualRectForText(
    const LayoutObject& layout_object,
    const FloatRect& text_bounds) {
  DCHECK(layout_object.IsSVGText() || layout_object.IsNGSVGText() ||
         layout_object.IsSVGInline());
  FloatRect visual_rect = ExtendTextBBoxWithStroke(layout_object, text_bounds);
  if (const ShadowList* text_shadow = layout_object.StyleRef().TextShadow())
    text_shadow->AdjustRectForShadow(visual_rect);
  return visual_rect;
}

bool SVGLayoutSupport::IntersectsClipPath(const LayoutObject& object,
                                          const FloatRect& reference_box,
                                          const HitTestLocation& location) {
  ClipPathOperation* clip_path_operation = object.StyleRef().ClipPath();
  if (!clip_path_operation)
    return true;
  if (clip_path_operation->GetType() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation& clip_path =
        To<ShapeClipPathOperation>(*clip_path_operation);
    return clip_path.GetPath(reference_box, 1)
        .Contains(location.TransformedPoint());
  }
  DCHECK_EQ(clip_path_operation->GetType(), ClipPathOperation::REFERENCE);
  SVGResourceClient* client = SVGResources::GetClient(object);
  auto* clipper = GetSVGResourceAsType(
      *client, To<ReferenceClipPathOperation>(*clip_path_operation));
  return !clipper || clipper->HitTestClipContent(reference_box, location);
}

DashArray SVGLayoutSupport::ResolveSVGDashArray(
    const SVGDashArray& svg_dash_array,
    const ComputedStyle& style,
    const SVGLengthContext& length_context) {
  DashArray dash_array;
  for (const Length& dash_length : svg_dash_array.data)
    dash_array.push_back(length_context.ValueForLength(dash_length, style));
  return dash_array;
}

void SVGLayoutSupport::ApplyStrokeStyleToStrokeData(StrokeData& stroke_data,
                                                    const ComputedStyle& style,
                                                    const LayoutObject& object,
                                                    float dash_scale_factor) {
  DCHECK(object.GetNode());
  DCHECK(object.GetNode()->IsSVGElement());

  SVGLengthContext length_context(To<SVGElement>(object.GetNode()));
  stroke_data.SetThickness(length_context.ValueForLength(style.StrokeWidth()));
  stroke_data.SetLineCap(style.CapStyle());
  stroke_data.SetLineJoin(style.JoinStyle());
  stroke_data.SetMiterLimit(style.StrokeMiterLimit());

  DashArray dash_array =
      ResolveSVGDashArray(*style.StrokeDashArray(), style, length_context);
  float dash_offset =
      length_context.ValueForLength(style.StrokeDashOffset(), style);
  // Apply scaling from 'pathLength'.
  if (dash_scale_factor != 1) {
    DCHECK_GE(dash_scale_factor, 0);
    dash_offset *= dash_scale_factor;
    for (auto& dash_item : dash_array)
      dash_item *= dash_scale_factor;
  }
  stroke_data.SetLineDash(dash_array, dash_offset);
}

bool SVGLayoutSupport::IsLayoutableTextNode(const LayoutObject* object) {
  DCHECK(object->IsText());
  // <br> is marked as text, but is not handled by the SVG layout code-path.
  return object->IsSVGInlineText() &&
         !To<LayoutSVGInlineText>(object)->HasEmptyText();
}

bool SVGLayoutSupport::WillIsolateBlendingDescendantsForStyle(
    const ComputedStyle& style) {
  return style.HasGroupingProperty(style.BoxReflect()) ||
         style.MaskerResource();
}

bool SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(
    const LayoutObject* object) {
  if (object->IsSVGHiddenContainer())
    return false;
  if (!object->IsSVGRoot() && !object->IsSVGContainer())
    return false;
  return WillIsolateBlendingDescendantsForStyle(object->StyleRef());
}

bool SVGLayoutSupport::IsIsolationRequired(const LayoutObject* object) {
  if (object->StyleRef().MaskerResource())
    return true;
  return WillIsolateBlendingDescendantsForObject(object) &&
         object->HasNonIsolatedBlendingDescendants();
}

AffineTransform::Transform
    SubtreeContentTransformScope::current_content_transformation_ =
        IDENTITY_TRANSFORM;

SubtreeContentTransformScope::SubtreeContentTransformScope(
    const AffineTransform& subtree_content_transformation)
    : saved_content_transformation_(current_content_transformation_) {
  AffineTransform content_transformation =
      subtree_content_transformation *
      AffineTransform(current_content_transformation_);
  content_transformation.CopyTransformTo(current_content_transformation_);
}

SubtreeContentTransformScope::~SubtreeContentTransformScope() {
  saved_content_transformation_.CopyTransformTo(
      current_content_transformation_);
}

AffineTransform SVGLayoutSupport::DeprecatedCalculateTransformToLayer(
    const LayoutObject* layout_object) {
  AffineTransform transform;
  while (layout_object) {
    transform = layout_object->LocalToSVGParentTransform() * transform;
    if (layout_object->IsSVGRoot())
      break;
    layout_object = layout_object->Parent();
  }

  // Continue walking up the layer tree, accumulating CSS transforms.
  // FIXME: this queries layer compositing state - which is not
  // supported during layout. Hence, the result may not include all CSS
  // transforms.
  PaintLayer* layer = layout_object ? layout_object->EnclosingLayer() : nullptr;
  while (layer && layer->IsAllowedToQueryCompositingState()) {
    // We can stop at compositing layers, to match the backing resolution.
    // FIXME: should we be computing the transform to the nearest composited
    // layer, or the nearest composited layer that does not paint into its
    // ancestor? I think this is the nearest composited ancestor since we will
    // inherit its transforms in the composited layer tree.
    if (layer->GetCompositingState() != kNotComposited)
      break;

    if (TransformationMatrix* layer_transform = layer->Transform())
      transform = layer_transform->ToAffineTransform() * transform;

    layer = layer->Parent();
  }

  return transform;
}

float SVGLayoutSupport::CalculateScreenFontSizeScalingFactor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);

  // FIXME: trying to compute a device space transform at record time is wrong.
  // All clients should be updated to avoid relying on this information, and the
  // method should be removed.
  AffineTransform ctm =
      DeprecatedCalculateTransformToLayer(layout_object) *
      SubtreeContentTransformScope::CurrentContentTransformation();
  ctm.Scale(
      layout_object->GetDocument().GetPage()->DeviceScaleFactorDeprecated());

  return clampTo<float>(sqrt((ctm.XScaleSquared() + ctm.YScaleSquared()) / 2));
}

static inline bool CompareCandidateDistance(const SearchCandidate& r1,
                                            const SearchCandidate& r2) {
  return r1.distance < r2.distance;
}

static inline float DistanceToChildLayoutObject(LayoutObject* child,
                                                const FloatPoint& point) {
  const AffineTransform& local_to_parent_transform =
      child->LocalToSVGParentTransform();
  if (!local_to_parent_transform.IsInvertible())
    return std::numeric_limits<float>::max();
  FloatPoint child_local_point =
      local_to_parent_transform.Inverse().MapPoint(point);
  return child->ObjectBoundingBox().SquaredDistanceTo(child_local_point);
}

static SearchCandidate SearchTreeForFindClosestLayoutSVGText(
    const LayoutObject* layout_object,
    const FloatPoint& point) {
  // Try to find the closest LayoutSVGText.
  SearchCandidate closest_text;
  Vector<SearchCandidate> candidates;

  // Find the closest LayoutSVGText on this tree level, and also collect any
  // containers that could contain LayoutSVGTexts that are closer.
  for (LayoutObject* child = layout_object->SlowLastChild(); child;
       child = child->PreviousSibling()) {
    if (child->IsSVGText()) {
      float distance = DistanceToChildLayoutObject(child, point);
      if (distance >= closest_text.distance)
        continue;
      candidates.clear();
      closest_text.layout_object = child;
      closest_text.distance = distance;
      continue;
    }

    if (child->IsSVGContainer() && !layout_object->IsSVGHiddenContainer()) {
      float distance = DistanceToChildLayoutObject(child, point);
      if (distance > closest_text.distance)
        continue;
      candidates.push_back(SearchCandidate(child, distance));
    }
  }

  // If a LayoutSVGText was found and there are no potentially closer sub-trees,
  // just return |closestText|.
  if (closest_text.layout_object && candidates.IsEmpty())
    return closest_text;

  std::stable_sort(candidates.begin(), candidates.end(),
                   CompareCandidateDistance);

  // Find the closest LayoutSVGText in the sub-trees in |candidates|.
  // If a LayoutSVGText is found that is strictly closer than any previous
  // candidate, then end the search.
  for (const SearchCandidate& search_candidate : candidates) {
    if (closest_text.distance < search_candidate.distance)
      break;
    LayoutObject* candidate_layout_object = search_candidate.layout_object;
    FloatPoint candidate_local_point =
        candidate_layout_object->LocalToSVGParentTransform().Inverse().MapPoint(
            point);

    SearchCandidate candidate_text = SearchTreeForFindClosestLayoutSVGText(
        candidate_layout_object, candidate_local_point);

    if (candidate_text.distance < closest_text.distance)
      closest_text = candidate_text;
  }

  return closest_text;
}

LayoutObject* SVGLayoutSupport::FindClosestLayoutSVGText(
    const LayoutObject* layout_object,
    const FloatPoint& point) {
  return SearchTreeForFindClosestLayoutSVGText(layout_object, point)
      .layout_object;
}

void SVGLayoutSupport::NotifySVGRootOfChangedCompositingReasons(
    const LayoutObject* object) {
  for (auto* ancestor = object->Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    if (ancestor->IsSVGRoot()) {
      To<LayoutSVGRoot>(ancestor)->NotifyDescendantCompositingReasonsChanged();
      break;
    }
  }
}

}  // namespace blink
