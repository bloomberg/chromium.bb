/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#include "core/layout/svg/LayoutSVGShape.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGShapePainter.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGPathElement.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/StrokeData.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

LayoutSVGShape::LayoutSVGShape(SVGGeometryElement* node)
    : LayoutSVGModelObject(node),
      // Default is false, the cached rects are empty from the beginning.
      needs_boundaries_update_(false),
      // Default is true, so we grab a Path object once from SVGGeometryElement.
      needs_shape_update_(true),
      // Default is true, so we grab a AffineTransform object once from
      // SVGGeometryElement.
      needs_transform_update_(true) {}

LayoutSVGShape::~LayoutSVGShape() {}

void LayoutSVGShape::CreatePath() {
  if (!path_)
    path_ = WTF::MakeUnique<Path>();
  *path_ = ToSVGGeometryElement(GetElement())->AsPath();
  if (rare_data_.get())
    rare_data_->cached_non_scaling_stroke_path_.Clear();
}

float LayoutSVGShape::DashScaleFactor() const {
  if (!StyleRef().SvgStyle().StrokeDashArray()->size())
    return 1;
  return ToSVGGeometryElement(*GetElement()).PathLengthScaleFactor();
}

void LayoutSVGShape::UpdateShapeFromElement() {
  CreatePath();

  fill_bounding_box_ = CalculateObjectBoundingBox();
  stroke_bounding_box_ = CalculateStrokeBoundingBox();
  if (GetElement())
    GetElement()->SetNeedsResizeObserverUpdate();
}

FloatRect LayoutSVGShape::HitTestStrokeBoundingBox() const {
  if (Style()->SvgStyle().HasStroke())
    return stroke_bounding_box_;

  // Implementation of
  // http://dev.w3.org/fxtf/css-masking-1/#compute-stroke-bounding-box
  // for the <rect> / <ellipse> / <circle> case except that we ignore whether
  // the stroke is none.

  FloatRect box = fill_bounding_box_;
  const float stroke_width = this->StrokeWidth();
  box.Inflate(stroke_width / 2);
  return box;
}

bool LayoutSVGShape::ShapeDependentStrokeContains(const FloatPoint& point) {
  DCHECK(path_);
  StrokeData stroke_data;
  SVGLayoutSupport::ApplyStrokeStyleToStrokeData(stroke_data, StyleRef(), *this,
                                                 DashScaleFactor());

  if (HasNonScalingStroke()) {
    AffineTransform non_scaling_transform = NonScalingStrokeTransform();
    Path* use_path = NonScalingStrokePath(path_.get(), non_scaling_transform);

    return use_path->StrokeContains(non_scaling_transform.MapPoint(point),
                                    stroke_data);
  }

  return path_->StrokeContains(point, stroke_data);
}

bool LayoutSVGShape::ShapeDependentFillContains(
    const FloatPoint& point,
    const WindRule fill_rule) const {
  return GetPath().Contains(point, fill_rule);
}

bool LayoutSVGShape::FillContains(const FloatPoint& point,
                                  bool requires_fill,
                                  const WindRule fill_rule) {
  if (!fill_bounding_box_.Contains(point))
    return false;

  if (requires_fill && !SVGPaintServer::ExistsForLayoutObject(*this, StyleRef(),
                                                              kApplyToFillMode))
    return false;

  return ShapeDependentFillContains(point, fill_rule);
}

bool LayoutSVGShape::StrokeContains(const FloatPoint& point,
                                    bool requires_stroke) {
  if (requires_stroke) {
    if (!StrokeBoundingBox().Contains(point))
      return false;

    if (!SVGPaintServer::ExistsForLayoutObject(*this, StyleRef(),
                                               kApplyToStrokeMode))
      return false;
  } else {
    if (!HitTestStrokeBoundingBox().Contains(point))
      return false;
  }

  return ShapeDependentStrokeContains(point);
}

void LayoutSVGShape::UpdateLocalTransform() {
  SVGGraphicsElement* graphics_element = ToSVGGraphicsElement(GetElement());
  if (graphics_element->HasTransform(SVGElement::kIncludeMotionTransform)) {
    local_transform_.SetTransform(graphics_element->CalculateTransform(
        SVGElement::kIncludeMotionTransform));
  } else {
    local_transform_ = AffineTransform();
  }
}

void LayoutSVGShape::UpdateLayout() {
  LayoutAnalyzer::Scope analyzer(*this);

  // Invalidate all resources of this client if our layout changed.
  if (EverHadLayout() && SelfNeedsLayout())
    SVGResourcesCache::ClientLayoutChanged(this);

  bool update_parent_boundaries = false;
  // updateShapeFromElement() also updates the object & stroke bounds - which
  // feeds into the visual rect - so we need to call it for both the
  // shape-update and the bounds-update flag.
  if (needs_shape_update_ || needs_boundaries_update_) {
    FloatRect old_object_bounding_box = ObjectBoundingBox();
    UpdateShapeFromElement();
    if (old_object_bounding_box != ObjectBoundingBox())
      SetShouldDoFullPaintInvalidation();
    needs_shape_update_ = false;

    local_visual_rect_ = StrokeBoundingBox();
    SVGLayoutSupport::AdjustVisualRectWithResources(this, local_visual_rect_);
    needs_boundaries_update_ = false;

    update_parent_boundaries = true;
  }

  if (needs_transform_update_) {
    UpdateLocalTransform();
    needs_transform_update_ = false;
    update_parent_boundaries = true;
  }

  // If our bounds changed, notify the parents.
  if (update_parent_boundaries)
    LayoutSVGModelObject::SetNeedsBoundariesUpdate();

  DCHECK(!needs_shape_update_);
  DCHECK(!needs_boundaries_update_);
  DCHECK(!needs_transform_update_);
  ClearNeedsLayout();
}

Path* LayoutSVGShape::NonScalingStrokePath(
    const Path* path,
    const AffineTransform& stroke_transform) const {
  LayoutSVGShapeRareData& rare_data = EnsureRareData();
  if (!rare_data.cached_non_scaling_stroke_path_.IsEmpty() &&
      stroke_transform == rare_data.cached_non_scaling_stroke_transform_)
    return &rare_data.cached_non_scaling_stroke_path_;

  rare_data.cached_non_scaling_stroke_path_ = *path;
  rare_data.cached_non_scaling_stroke_path_.Transform(stroke_transform);
  rare_data.cached_non_scaling_stroke_transform_ = stroke_transform;
  return &rare_data.cached_non_scaling_stroke_path_;
}

AffineTransform LayoutSVGShape::NonScalingStrokeTransform() const {
  // Compute the CTM to the SVG root. This should probably be the CTM all the
  // way to the "canvas" of the page ("host" coordinate system), but with our
  // current approach of applying/painting non-scaling-stroke, that can break in
  // unpleasant ways (see crbug.com/747708 for an example.) Maybe it would be
  // better to apply this effect during rasterization?
  const LayoutSVGRoot* svg_root = SVGLayoutSupport::FindTreeRootObject(this);
  AffineTransform t;
  t.Scale(1 / StyleRef().EffectiveZoom())
      .Multiply(LocalToAncestorTransform(svg_root).ToAffineTransform());
  // Width of non-scaling stroke is independent of translation, so zero it out
  // here.
  t.SetE(0);
  t.SetF(0);
  return t;
}

void LayoutSVGShape::Paint(const PaintInfo& paint_info,
                           const LayoutPoint&) const {
  SVGShapePainter(*this).Paint(paint_info);
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void LayoutSVGShape::AddOutlineRects(Vector<LayoutRect>& rects,
                                     const LayoutPoint&,
                                     IncludeBlockVisualOverflowOrNot) const {
  rects.push_back(LayoutRect(VisualRectInLocalSVGCoordinates()));
}

bool LayoutSVGShape::NodeAtFloatPoint(HitTestResult& result,
                                      const FloatPoint& point_in_parent,
                                      HitTestAction hit_test_action) {
  // We only draw in the foreground phase, so we only hit-test then.
  if (hit_test_action != kHitTestForeground)
    return false;

  FloatPoint local_point;
  if (!SVGLayoutSupport::TransformToUserSpaceAndCheckClipping(
          *this, LocalToSVGParentTransform(), point_in_parent, local_point))
    return false;

  PointerEventsHitRules hit_rules(
      PointerEventsHitRules::SVG_GEOMETRY_HITTESTING,
      result.GetHitTestRequest(), Style()->PointerEvents());
  if (NodeAtFloatPointInternal(result.GetHitTestRequest(), local_point,
                               hit_rules)) {
    const LayoutPoint& local_layout_point = LayoutPoint(local_point);
    UpdateHitTestResult(result, local_layout_point);
    if (result.AddNodeToListBasedTestResult(GetElement(), local_layout_point) ==
        kStopHitTesting)
      return true;
  }

  return false;
}

bool LayoutSVGShape::NodeAtFloatPointInternal(const HitTestRequest& request,
                                              const FloatPoint& local_point,
                                              PointerEventsHitRules hit_rules) {
  bool is_visible = (Style()->Visibility() == EVisibility::kVisible);
  if (is_visible || !hit_rules.require_visible) {
    const SVGComputedStyle& svg_style = Style()->SvgStyle();
    WindRule fill_rule = svg_style.FillRule();
    if (request.SvgClipContent())
      fill_rule = svg_style.ClipRule();
    if ((hit_rules.can_hit_bounding_box &&
         ObjectBoundingBox().Contains(local_point)) ||
        (hit_rules.can_hit_stroke &&
         (svg_style.HasStroke() || !hit_rules.require_stroke) &&
         StrokeContains(local_point, hit_rules.require_stroke)) ||
        (hit_rules.can_hit_fill &&
         (svg_style.HasFill() || !hit_rules.require_fill) &&
         FillContains(local_point, hit_rules.require_fill, fill_rule)))
      return true;
  }
  return false;
}

FloatRect LayoutSVGShape::CalculateObjectBoundingBox() const {
  return GetPath().BoundingRect();
}

FloatRect LayoutSVGShape::CalculateStrokeBoundingBox() const {
  DCHECK(path_);
  FloatRect stroke_bounding_box = fill_bounding_box_;

  if (Style()->SvgStyle().HasStroke()) {
    StrokeData stroke_data;
    SVGLayoutSupport::ApplyStrokeStyleToStrokeData(stroke_data, StyleRef(),
                                                   *this, DashScaleFactor());
    if (HasNonScalingStroke()) {
      AffineTransform non_scaling_transform = NonScalingStrokeTransform();
      if (non_scaling_transform.IsInvertible()) {
        Path* use_path =
            NonScalingStrokePath(path_.get(), non_scaling_transform);
        FloatRect stroke_bounding_rect =
            use_path->StrokeBoundingRect(stroke_data);
        stroke_bounding_rect =
            non_scaling_transform.Inverse().MapRect(stroke_bounding_rect);
        stroke_bounding_box.Unite(stroke_bounding_rect);
      }
    } else {
      stroke_bounding_box.Unite(GetPath().StrokeBoundingRect(stroke_data));
    }
  }

  return stroke_bounding_box;
}

float LayoutSVGShape::StrokeWidth() const {
  SVGLengthContext length_context(GetElement());
  return length_context.ValueForLength(Style()->SvgStyle().StrokeWidth());
}

LayoutSVGShapeRareData& LayoutSVGShape::EnsureRareData() const {
  if (!rare_data_)
    rare_data_ = WTF::MakeUnique<LayoutSVGShapeRareData>();
  return *rare_data_.get();
}

LayoutUnit LayoutSVGShape::VisualRectOutsetForRasterEffects() const {
  // Account for raster expansions due to SVG stroke hairline raster effects.
  if (StyleRef().SvgStyle().HasVisibleStroke()) {
    LayoutUnit outset(0.5f);
    if (StyleRef().SvgStyle().CapStyle() != kButtCap)
      outset += 0.5f;
    return outset;
  }
  return LayoutUnit();
}

}  // namespace blink
