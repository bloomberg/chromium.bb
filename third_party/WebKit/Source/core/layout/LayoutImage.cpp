/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
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
 *
 */

#include "core/layout/LayoutImage.h"

#include "core/dom/PseudoElement.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html_names.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutView.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/paint/ImagePainter.h"
#include "core/svg/graphics/SVGImage.h"

namespace blink {

using namespace HTMLNames;

LayoutImage::LayoutImage(Element* element)
    : LayoutReplaced(element, LayoutSize()),
      did_increment_visually_non_empty_pixel_count_(false),
      is_generated_content_(false),
      image_device_pixel_ratio_(1.0f) {}

LayoutImage* LayoutImage::CreateAnonymous(PseudoElement& pseudo) {
  LayoutImage* image = new LayoutImage(nullptr);
  image->SetDocumentForAnonymous(&pseudo.GetDocument());
  return image;
}

LayoutImage::~LayoutImage() {}

void LayoutImage::WillBeDestroyed() {
  DCHECK(image_resource_);
  image_resource_->Shutdown();
  LayoutReplaced::WillBeDestroyed();
}

void LayoutImage::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  LayoutReplaced::StyleDidChange(diff, old_style);

  bool old_orientation = old_style
                             ? old_style->RespectImageOrientation()
                             : ComputedStyle::InitialRespectImageOrientation();
  if (Style() && Style()->RespectImageOrientation() != old_orientation)
    IntrinsicSizeChanged();
}

void LayoutImage::SetImageResource(LayoutImageResource* image_resource) {
  DCHECK(!image_resource_);
  image_resource_ = image_resource;
  image_resource_->Initialize(this);
}

void LayoutImage::ImageChanged(WrappedImagePtr new_image, const IntRect* rect) {
  DCHECK(View());
  DCHECK(View()->GetFrameView());
  if (DocumentBeingDestroyed())
    return;

  if (HasBoxDecorationBackground() || HasMask() || HasShapeOutside() ||
      HasReflection())
    LayoutReplaced::ImageChanged(new_image, rect);

  if (!image_resource_)
    return;

  if (new_image != image_resource_->ImagePtr())
    return;

  if (IsGeneratedContent() && IsHTMLImageElement(GetNode()) &&
      image_resource_->ErrorOccurred()) {
    ToHTMLImageElement(GetNode())->EnsureFallbackForGeneratedContent();
    return;
  }

  // Per the spec, we let the server-sent header override srcset/other sources
  // of dpr.
  // https://github.com/igrigorik/http-client-hints/blob/master/draft-grigorik-http-client-hints-01.txt#L255
  if (image_resource_->CachedImage() &&
      image_resource_->CachedImage()->HasDevicePixelRatioHeaderValue()) {
    UseCounter::Count(&(View()->GetFrameView()->GetFrame()),
                      WebFeature::kClientHintsContentDPR);
    image_device_pixel_ratio_ =
        1 / image_resource_->CachedImage()->DevicePixelRatioHeaderValue();
  }

  if (!did_increment_visually_non_empty_pixel_count_) {
    // At a zoom level of 1 the image is guaranteed to have an integer size.
    View()->GetFrameView()->IncrementVisuallyNonEmptyPixelCount(
        FlooredIntSize(image_resource_->ImageSize(1.0f)));
    did_increment_visually_non_empty_pixel_count_ = true;
  }

  InvalidatePaintAndMarkForLayoutIfNeeded();
}

void LayoutImage::UpdateIntrinsicSizeIfNeeded(const LayoutSize& new_size) {
  if (image_resource_->ErrorOccurred() || !image_resource_->HasImage())
    return;
  SetIntrinsicSize(new_size);
}

void LayoutImage::InvalidatePaintAndMarkForLayoutIfNeeded() {
  LayoutSize old_intrinsic_size = IntrinsicSize();
  LayoutSize new_intrinsic_size =
      image_resource_->ImageSize(Style()->EffectiveZoom());
  UpdateIntrinsicSizeIfNeeded(new_intrinsic_size);

  // In the case of generated image content using :before/:after/content, we
  // might not be in the layout tree yet. In that case, we just need to update
  // our intrinsic size. layout() will be called after we are inserted in the
  // tree which will take care of what we are doing here.
  if (!ContainingBlock())
    return;

  bool image_source_has_changed_size = old_intrinsic_size != new_intrinsic_size;
  if (image_source_has_changed_size)
    SetPreferredLogicalWidthsDirty();

  // If the actual area occupied by the image has changed and it is not
  // constrained by style then a layout is required.
  bool image_size_is_constrained = Style()->LogicalWidth().IsSpecified() &&
                                   Style()->LogicalHeight().IsSpecified();

  // FIXME: We only need to recompute the containing block's preferred size if
  // the containing block's size depends on the image's size (i.e., the
  // container uses shrink-to-fit sizing). There's no easy way to detect that
  // shrink-to-fit is needed, always force a layout.
  bool containing_block_needs_to_recompute_preferred_size =
      Style()->LogicalWidth().IsPercentOrCalc() ||
      Style()->LogicalMaxWidth().IsPercentOrCalc() ||
      Style()->LogicalMinWidth().IsPercentOrCalc();

  if (image_source_has_changed_size &&
      (!image_size_is_constrained ||
       containing_block_needs_to_recompute_preferred_size)) {
    SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kSizeChanged);
    return;
  }

  SetShouldDoFullPaintInvalidationWithoutGeometryChange(
      ImageResource() && ImageResource()->MaybeAnimated()
          ? PaintInvalidationReason::kDelayedFull
          : PaintInvalidationReason::kImage);

  // Tell any potential compositing layers that the image needs updating.
  ContentChanged(kImageChanged);
}

void LayoutImage::ImageNotifyFinished(ImageResourceContent* new_image) {
  if (!image_resource_)
    return;

  if (DocumentBeingDestroyed())
    return;

  InvalidateBackgroundObscurationStatus();

  if (new_image == image_resource_->CachedImage()) {
    // tell any potential compositing layers
    // that the image is done and they can reference it directly.
    ContentChanged(kImageChanged);
  }
}

void LayoutImage::PaintReplaced(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) const {
  ImagePainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutImage::Paint(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset) const {
  ImagePainter(*this).Paint(paint_info, paint_offset);
}

void LayoutImage::AreaElementFocusChanged(HTMLAreaElement* area_element) {
  DCHECK_EQ(area_element->ImageElement(), GetNode());

  if (area_element->GetPath(this).IsEmpty())
    return;

  InvalidatePaintAndMarkForLayoutIfNeeded();
}

bool LayoutImage::ForegroundIsKnownToBeOpaqueInRect(
    const LayoutRect& local_rect,
    unsigned) const {
  if (!image_resource_->HasImage() || image_resource_->ErrorOccurred())
    return false;
  if (!image_resource_->CachedImage() ||
      !image_resource_->CachedImage()->IsLoaded())
    return false;
  if (!ContentBoxRect().Contains(local_rect))
    return false;
  EFillBox background_clip = Style()->BackgroundClip();
  // Background paints under borders.
  if (background_clip == kBorderFillBox && Style()->HasBorder() &&
      !Style()->BorderObscuresBackground())
    return false;
  // Background shows in padding area.
  if ((background_clip == kBorderFillBox ||
       background_clip == kPaddingFillBox) &&
      Style()->HasPadding())
    return false;
  // Object-position may leave parts of the content box empty, regardless of the
  // value of object-fit.
  if (Style()->ObjectPosition() != ComputedStyle::InitialObjectPosition())
    return false;
  // Object-fit may leave parts of the content box empty.
  EObjectFit object_fit = Style()->GetObjectFit();
  if (object_fit != EObjectFit::kFill && object_fit != EObjectFit::kCover)
    return false;
  // Check for image with alpha.
  TRACE_EVENT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data",
      InspectorPaintImageEvent::Data(this, *image_resource_->CachedImage()));
  return image_resource_->CachedImage()
      ->GetImage()
      ->CurrentFrameKnownToBeOpaque(Image::kPreCacheMetadata);
}

bool LayoutImage::ComputeBackgroundIsKnownToBeObscured() const {
  if (!StyleRef().HasBackground())
    return false;

  LayoutRect painted_extent;
  if (!GetBackgroundPaintedExtent(painted_extent))
    return false;
  return ForegroundIsKnownToBeOpaqueInRect(painted_extent, 0);
}

LayoutUnit LayoutImage::MinimumReplacedHeight() const {
  return image_resource_->ErrorOccurred() ? IntrinsicSize().Height()
                                          : LayoutUnit();
}

HTMLMapElement* LayoutImage::ImageMap() const {
  HTMLImageElement* i = ToHTMLImageElementOrNull(GetNode());
  return i ? i->GetTreeScope().GetImageMap(i->FastGetAttribute(usemapAttr))
           : nullptr;
}

bool LayoutImage::NodeAtPoint(HitTestResult& result,
                              const HitTestLocation& location_in_container,
                              const LayoutPoint& accumulated_offset,
                              HitTestAction hit_test_action) {
  HitTestResult temp_result(result.GetHitTestRequest(),
                            result.GetHitTestLocation());
  bool inside = LayoutReplaced::NodeAtPoint(
      temp_result, location_in_container, accumulated_offset, hit_test_action);

  if (!inside && result.GetHitTestRequest().ListBased())
    result.Append(temp_result);
  if (inside)
    result = temp_result;
  return inside;
}

void LayoutImage::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  LayoutReplaced::ComputeIntrinsicSizingInfo(intrinsic_sizing_info);

  // Our intrinsicSize is empty if we're laying out generated images with
  // relative width/height. Figure out the right intrinsic size to use.
  if (intrinsic_sizing_info.size.IsEmpty() &&
      image_resource_->ImageHasRelativeSize()) {
    LayoutObject* containing_block =
        IsOutOfFlowPositioned() ? Container() : this->ContainingBlock();
    if (containing_block->IsBox()) {
      LayoutBox* box = ToLayoutBox(containing_block);
      intrinsic_sizing_info.size.SetWidth(
          box->AvailableLogicalWidth().ToFloat());
      intrinsic_sizing_info.size.SetHeight(
          box->AvailableLogicalHeight(kIncludeMarginBorderPadding).ToFloat());
    }
  }
  // Don't compute an intrinsic ratio to preserve historical WebKit behavior if
  // we're painting alt text and/or a broken image.
  // Video is excluded from this behavior because video elements have a default
  // aspect ratio that a failed poster image load should not override.
  if (image_resource_ && image_resource_->ErrorOccurred() && !IsVideo()) {
    intrinsic_sizing_info.aspect_ratio = FloatSize(1, 1);
    return;
  }
}

bool LayoutImage::NeedsPreferredWidthsRecalculation() const {
  if (LayoutReplaced::NeedsPreferredWidthsRecalculation())
    return true;
  return EmbeddedReplacedContent();
}

LayoutReplaced* LayoutImage::EmbeddedReplacedContent() const {
  if (!image_resource_)
    return nullptr;

  ImageResourceContent* cached_image = image_resource_->CachedImage();
  // TODO(japhet): This shouldn't need to worry about cache validation.
  // https://crbug.com/761026
  if (cached_image && !cached_image->IsCacheValidator() &&
      cached_image->GetImage() && cached_image->GetImage()->IsSVGImage())
    return ToSVGImage(cached_image->GetImage())->EmbeddedReplacedContent();

  return nullptr;
}

}  // namespace blink
