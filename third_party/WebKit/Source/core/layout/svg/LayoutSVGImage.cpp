/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "core/layout/svg/LayoutSVGImage.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGImagePainter.h"
#include "core/svg/SVGImageElement.h"
#include "platform/LengthFunctions.h"
#include "platform/graphics/paint/PaintRecord.h"

namespace blink {

LayoutSVGImage::LayoutSVGImage(SVGImageElement* impl)
    : LayoutSVGModelObject(impl),
      needs_boundaries_update_(true),
      needs_transform_update_(true),
      image_resource_(LayoutImageResource::Create()) {
  image_resource_->Initialize(this);
}

LayoutSVGImage::~LayoutSVGImage() {}

void LayoutSVGImage::WillBeDestroyed() {
  ImageQualityController::Remove(*this);
  image_resource_->Shutdown();
  LayoutSVGModelObject::WillBeDestroyed();
}

static float ResolveWidthForRatio(float height,
                                  const FloatSize& intrinsic_ratio) {
  return height * intrinsic_ratio.Width() / intrinsic_ratio.Height();
}

static float ResolveHeightForRatio(float width,
                                   const FloatSize& intrinsic_ratio) {
  return width * intrinsic_ratio.Height() / intrinsic_ratio.Width();
}

FloatSize LayoutSVGImage::CalculateObjectSize() const {
  ImageResourceContent* cached_image = image_resource_->CachedImage();
  if (!cached_image || cached_image->ErrorOccurred() ||
      !cached_image->IsSizeAvailable())
    return object_bounding_box_.Size();

  FloatSize intrinsic_size = FloatSize(cached_image->GetImage()->Size());
  if (StyleRef().Width().IsAuto() && StyleRef().Height().IsAuto())
    return intrinsic_size;

  if (StyleRef().Height().IsAuto())
    return FloatSize(
        object_bounding_box_.Width(),
        ResolveHeightForRatio(object_bounding_box_.Width(), intrinsic_size));

  DCHECK(StyleRef().Width().IsAuto());
  return FloatSize(
      ResolveWidthForRatio(object_bounding_box_.Height(), intrinsic_size),
      object_bounding_box_.Height());
}

bool LayoutSVGImage::UpdateBoundingBox() {
  FloatRect old_object_bounding_box = object_bounding_box_;

  SVGLengthContext length_context(GetElement());
  object_bounding_box_ = FloatRect(
      length_context.ValueForLength(StyleRef().SvgStyle().X(), StyleRef(),
                                    SVGLengthMode::kWidth),
      length_context.ValueForLength(StyleRef().SvgStyle().Y(), StyleRef(),
                                    SVGLengthMode::kHeight),
      length_context.ValueForLength(StyleRef().Width(), StyleRef(),
                                    SVGLengthMode::kWidth),
      length_context.ValueForLength(StyleRef().Height(), StyleRef(),
                                    SVGLengthMode::kHeight));

  if (StyleRef().Width().IsAuto() || StyleRef().Height().IsAuto())
    object_bounding_box_.SetSize(CalculateObjectSize());

  if (old_object_bounding_box != object_bounding_box_) {
    SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kImage);
    needs_boundaries_update_ = true;
  }

  if (GetElement())
    GetElement()->SetNeedsResizeObserverUpdate();

  return old_object_bounding_box.Size() != object_bounding_box_.Size();
}

void LayoutSVGImage::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  // Invalidate all resources of this client if our layout changed.
  if (EverHadLayout() && SelfNeedsLayout())
    SVGResourcesCache::ClientLayoutChanged(this);

  UpdateBoundingBox();

  bool update_parent_boundaries = false;
  if (needs_transform_update_) {
    local_transform_ =
        toSVGImageElement(GetElement())
            ->CalculateTransform(SVGElement::kIncludeMotionTransform);
    needs_transform_update_ = false;
    update_parent_boundaries = true;
  }

  if (needs_boundaries_update_) {
    local_visual_rect_ = object_bounding_box_;
    SVGLayoutSupport::AdjustVisualRectWithResources(this, local_visual_rect_);
    needs_boundaries_update_ = false;
    update_parent_boundaries = true;
  }

  // If our bounds changed, notify the parents.
  if (update_parent_boundaries)
    LayoutSVGModelObject::SetNeedsBoundariesUpdate();

  DCHECK(!needs_boundaries_update_);
  DCHECK(!needs_transform_update_);
  ClearNeedsLayout();
}

void LayoutSVGImage::Paint(const PaintInfo& paint_info,
                           const LayoutPoint&) const {
  SVGImagePainter(*this).Paint(paint_info);
}

bool LayoutSVGImage::NodeAtFloatPoint(HitTestResult& result,
                                      const FloatPoint& point_in_parent,
                                      HitTestAction hit_test_action) {
  // We only draw in the forground phase, so we only hit-test then.
  if (hit_test_action != kHitTestForeground)
    return false;

  PointerEventsHitRules hit_rules(PointerEventsHitRules::SVG_IMAGE_HITTESTING,
                                  result.GetHitTestRequest(),
                                  Style()->PointerEvents());
  bool is_visible = (Style()->Visibility() == EVisibility::kVisible);
  if (is_visible || !hit_rules.require_visible) {
    FloatPoint local_point;
    if (!SVGLayoutSupport::TransformToUserSpaceAndCheckClipping(
            *this, LocalToSVGParentTransform(), point_in_parent, local_point))
      return false;

    if (hit_rules.can_hit_fill || hit_rules.can_hit_bounding_box) {
      if (object_bounding_box_.Contains(local_point)) {
        const LayoutPoint& local_layout_point = LayoutPoint(local_point);
        UpdateHitTestResult(result, local_layout_point);
        if (result.AddNodeToListBasedTestResult(
                GetElement(), local_layout_point) == kStopHitTesting)
          return true;
      }
    }
  }

  return false;
}

void LayoutSVGImage::ImageChanged(WrappedImagePtr, const IntRect*) {
  // Notify parent resources that we've changed. This also invalidates
  // references from resources (filters) that may have a cached
  // representation of this image/layout object.
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(this,
                                                                         false);

  if (StyleRef().Width().IsAuto() || StyleRef().Height().IsAuto()) {
    if (UpdateBoundingBox())
      SetNeedsLayout(LayoutInvalidationReason::kSizeChanged);
  }

  SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kImage);
}

void LayoutSVGImage::AddOutlineRects(Vector<LayoutRect>& rects,
                                     const LayoutPoint&,
                                     IncludeBlockVisualOverflowOrNot) const {
  // this is called from paint() after the localTransform has already been
  // applied
  rects.push_back(LayoutRect(VisualRectInLocalSVGCoordinates()));
}

}  // namespace blink
