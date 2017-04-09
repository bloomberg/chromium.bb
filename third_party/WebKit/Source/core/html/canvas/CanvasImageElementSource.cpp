// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasImageElementSource.h"

#include "core/layout/LayoutObject.h"
#include "core/loader/ImageLoader.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

ImageResourceContent* CanvasImageElementSource::CachedImage() const {
  return GetImageLoader().GetImage();
}

const Element& CanvasImageElementSource::GetElement() const {
  return *GetImageLoader().GetElement();
}

bool CanvasImageElementSource::IsSVGSource() const {
  return CachedImage() && CachedImage()->GetImage()->IsSVGImage();
}

PassRefPtr<Image> CanvasImageElementSource::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize& default_object_size) const {
  if (!GetImageLoader().ImageComplete() || !CachedImage()) {
    *status = kIncompleteSourceImageStatus;
    return nullptr;
  }

  if (CachedImage()->ErrorOccurred()) {
    *status = kUndecodableSourceImageStatus;
    return nullptr;
  }

  RefPtr<Image> source_image;
  if (CachedImage()->GetImage()->IsSVGImage()) {
    UseCounter::Count(GetElement().GetDocument(), UseCounter::kSVGInCanvas2D);
    SVGImage* svg_image = ToSVGImage(CachedImage()->GetImage());
    IntSize image_size =
        RoundedIntSize(svg_image->ConcreteObjectSize(default_object_size));
    source_image = SVGImageForContainer::Create(
        svg_image, image_size, 1,
        GetElement().GetDocument().CompleteURL(GetElement().ImageSourceURL()));
  } else {
    source_image = CachedImage()->GetImage();
  }

  *status = kNormalSourceImageStatus;
  return source_image->ImageForDefaultFrame();
}

bool CanvasImageElementSource::WouldTaintOrigin(
    SecurityOrigin* destination_security_origin) const {
  return CachedImage() &&
         !CachedImage()->IsAccessAllowed(destination_security_origin);
}

FloatSize CanvasImageElementSource::ElementSize(
    const FloatSize& default_object_size) const {
  ImageResourceContent* image = CachedImage();
  if (!image)
    return FloatSize();

  if (image->GetImage() && image->GetImage()->IsSVGImage()) {
    return ToSVGImage(CachedImage()->GetImage())
        ->ConcreteObjectSize(default_object_size);
  }

  return FloatSize(image->ImageSize(LayoutObject::ShouldRespectImageOrientation(
                                        GetElement().GetLayoutObject()),
                                    1.0f));
}

FloatSize CanvasImageElementSource::DefaultDestinationSize(
    const FloatSize& default_object_size) const {
  ImageResourceContent* image = CachedImage();
  if (!image)
    return FloatSize();

  if (image->GetImage() && image->GetImage()->IsSVGImage()) {
    return ToSVGImage(CachedImage()->GetImage())
        ->ConcreteObjectSize(default_object_size);
  }

  LayoutSize size;
  size = image->ImageSize(LayoutObject::ShouldRespectImageOrientation(
                              GetElement().GetLayoutObject()),
                          1.0f);
  return FloatSize(size);
}

bool CanvasImageElementSource::IsAccelerated() const {
  return false;
}

const KURL& CanvasImageElementSource::SourceURL() const {
  return CachedImage()->GetResponse().Url();
}

int CanvasImageElementSource::SourceWidth() {
  SourceImageStatus status;
  RefPtr<Image> image = GetSourceImageForCanvas(&status, kPreferNoAcceleration,
                                                kSnapshotReasonUnknown,
                                                SourceDefaultObjectSize());
  return image->width();
}

int CanvasImageElementSource::SourceHeight() {
  SourceImageStatus status;
  RefPtr<Image> image = GetSourceImageForCanvas(&status, kPreferNoAcceleration,
                                                kSnapshotReasonUnknown,
                                                SourceDefaultObjectSize());
  return image->height();
}

bool CanvasImageElementSource::IsOpaque() const {
  Image* image = const_cast<Element&>(GetElement()).ImageContents();
  return image && image->CurrentFrameKnownToBeOpaque();
}

}  // namespace blink
