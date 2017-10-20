/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/style/StyleFetchedImageSet.h"

#include "core/css/CSSImageSetValue.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

StyleFetchedImageSet::StyleFetchedImageSet(ImageResourceContent* image,
                                           float image_scale_factor,
                                           CSSImageSetValue* value,
                                           const KURL& url)
    : best_fit_image_(image),
      image_scale_factor_(image_scale_factor),
      image_set_value_(value),
      url_(url) {
  is_image_resource_set_ = true;
  best_fit_image_->AddObserver(this);
}

StyleFetchedImageSet::~StyleFetchedImageSet() {}

void StyleFetchedImageSet::Dispose() {
  best_fit_image_->RemoveObserver(this);
  best_fit_image_ = nullptr;
}

WrappedImagePtr StyleFetchedImageSet::Data() const {
  return best_fit_image_.Get();
}

ImageResourceContent* StyleFetchedImageSet::CachedImage() const {
  return best_fit_image_.Get();
}

CSSValue* StyleFetchedImageSet::CssValue() const {
  return image_set_value_;
}

CSSValue* StyleFetchedImageSet::ComputedCSSValue() const {
  return image_set_value_->ValueWithURLsMadeAbsolute();
}

bool StyleFetchedImageSet::CanRender() const {
  return !best_fit_image_->ErrorOccurred() &&
         !best_fit_image_->GetImage()->IsNull();
}

bool StyleFetchedImageSet::IsLoaded() const {
  return best_fit_image_->IsLoaded();
}

bool StyleFetchedImageSet::ErrorOccurred() const {
  return best_fit_image_->ErrorOccurred();
}

LayoutSize StyleFetchedImageSet::ImageSize(
    const Document&,
    float multiplier,
    const LayoutSize& default_object_size) const {
  if (best_fit_image_->GetImage() && best_fit_image_->GetImage()->IsSVGImage())
    return ImageSizeForSVGImage(ToSVGImage(best_fit_image_->GetImage()),
                                multiplier, default_object_size);

  // Image orientation should only be respected for content images,
  // not decorative ones such as StyleImage (backgrounds,
  // border-image, etc.)
  //
  // https://drafts.csswg.org/css-images-3/#the-image-orientation
  LayoutSize natural_size(
      best_fit_image_->IntrinsicSize(kDoNotRespectImageOrientation));
  LayoutSize scaled_image_size(ApplyZoom(natural_size, multiplier));
  scaled_image_size.Scale(1 / image_scale_factor_);
  return scaled_image_size;
}

bool StyleFetchedImageSet::ImageHasRelativeSize() const {
  return best_fit_image_->ImageHasRelativeSize();
}

bool StyleFetchedImageSet::UsesImageContainerSize() const {
  return best_fit_image_->UsesImageContainerSize();
}

void StyleFetchedImageSet::AddClient(ImageResourceObserver* observer) {
  best_fit_image_->AddObserver(observer);
}

void StyleFetchedImageSet::RemoveClient(ImageResourceObserver* observer) {
  best_fit_image_->RemoveObserver(observer);
}

scoped_refptr<Image> StyleFetchedImageSet::GetImage(
    const ImageResourceObserver&,
    const Document&,
    const ComputedStyle& style,
    const IntSize& container_size,
    const LayoutSize* logical_size) const {
  if (!best_fit_image_->GetImage()->IsSVGImage())
    return best_fit_image_->GetImage();

  return SVGImageForContainer::Create(ToSVGImage(best_fit_image_->GetImage()),
                                      container_size, style.EffectiveZoom(),
                                      url_);
}

bool StyleFetchedImageSet::KnownToBeOpaque(const Document&,
                                           const ComputedStyle&) const {
  return best_fit_image_->GetImage()->CurrentFrameKnownToBeOpaque(
      Image::kPreCacheMetadata);
}

void StyleFetchedImageSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(best_fit_image_);
  visitor->Trace(image_set_value_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
