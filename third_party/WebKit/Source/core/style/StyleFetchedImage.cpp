/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "core/style/StyleFetchedImage.h"

#include "core/css/CSSImageValue.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

StyleFetchedImage::StyleFetchedImage(ImageResourceContent* image,
                                     const Document& document,
                                     const KURL& url)
    : image_(image), document_(&document), url_(url) {
  is_image_resource_ = true;
  image_->AddObserver(this);
  // ResourceFetcher is not determined from StyleFetchedImage and it is
  // impossible to send a request for refetching.
  image_->SetNotRefetchableDataFromDiskCache();
}

StyleFetchedImage::~StyleFetchedImage() {}

void StyleFetchedImage::Dispose() {
  image_->RemoveObserver(this);
  image_ = nullptr;
}

WrappedImagePtr StyleFetchedImage::Data() const {
  return image_.Get();
}

ImageResourceContent* StyleFetchedImage::CachedImage() const {
  return image_.Get();
}

CSSValue* StyleFetchedImage::CssValue() const {
  return CSSImageValue::Create(url_, const_cast<StyleFetchedImage*>(this));
}

CSSValue* StyleFetchedImage::ComputedCSSValue() const {
  return CssValue();
}

bool StyleFetchedImage::CanRender() const {
  return !image_->ErrorOccurred() && !image_->GetImage()->IsNull();
}

bool StyleFetchedImage::IsLoaded() const {
  return image_->IsLoaded();
}

bool StyleFetchedImage::ErrorOccurred() const {
  return image_->ErrorOccurred();
}

LayoutSize StyleFetchedImage::ImageSize(
    const Document&,
    float multiplier,
    const LayoutSize& default_object_size) const {
  if (image_->GetImage() && image_->GetImage()->IsSVGImage())
    return ImageSizeForSVGImage(ToSVGImage(image_->GetImage()), multiplier,
                                default_object_size);

  // Image orientation should only be respected for content images,
  // not decorative images such as StyleImage (backgrounds,
  // border-image, etc.)
  //
  // https://drafts.csswg.org/css-images-3/#the-image-orientation
  LayoutSize size(image_->IntrinsicSize(kDoNotRespectImageOrientation));
  return ApplyZoom(size, multiplier);
}

bool StyleFetchedImage::ImageHasRelativeSize() const {
  return image_->ImageHasRelativeSize();
}

bool StyleFetchedImage::UsesImageContainerSize() const {
  return image_->UsesImageContainerSize();
}

void StyleFetchedImage::AddClient(ImageResourceObserver* observer) {
  image_->AddObserver(observer);
}

void StyleFetchedImage::RemoveClient(ImageResourceObserver* observer) {
  image_->RemoveObserver(observer);
}

void StyleFetchedImage::ImageNotifyFinished(ImageResourceContent*) {
  if (document_ && image_ && image_->GetImage() &&
      image_->GetImage()->IsSVGImage())
    ToSVGImage(image_->GetImage())->UpdateUseCounters(*document_);

  if (image_ && image_->GetImage()) {
    Image::RecordCheckerableImageUMA(*image_->GetImage(),
                                     Image::ImageType::kCss);
  }

  // Oilpan: do not prolong the Document's lifetime.
  document_.Clear();
}

RefPtr<Image> StyleFetchedImage::GetImage(
    const ImageResourceObserver&,
    const Document&,
    const ComputedStyle& style,
    const IntSize& container_size,
    const LayoutSize* logical_size) const {
  if (!image_->GetImage()->IsSVGImage())
    return image_->GetImage();

  return SVGImageForContainer::Create(ToSVGImage(image_->GetImage()),
                                      container_size, style.EffectiveZoom(),
                                      url_);
}

bool StyleFetchedImage::KnownToBeOpaque(const Document&,
                                        const ComputedStyle&) const {
  return image_->GetImage()->CurrentFrameKnownToBeOpaque(
      Image::kPreCacheMetadata);
}

void StyleFetchedImage::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_);
  visitor->Trace(document_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
