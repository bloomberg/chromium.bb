// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/ImageElementBase.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/ImageLoader.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

ImageResourceContent* ImageElementBase::CachedImage() const {
  return GetImageLoader().GetImage();
}

const Element& ImageElementBase::GetElement() const {
  return *GetImageLoader().GetElement();
}

bool ImageElementBase::IsSVGSource() const {
  return CachedImage() && CachedImage()->GetImage()->IsSVGImage();
}

RefPtr<Image> ImageElementBase::GetSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize& default_object_size) {
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
    UseCounter::Count(GetElement().GetDocument(), WebFeature::kSVGInCanvas2D);
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

bool ImageElementBase::WouldTaintOrigin(
    SecurityOrigin* destination_security_origin) const {
  return CachedImage() &&
         !CachedImage()->IsAccessAllowed(destination_security_origin);
}

FloatSize ImageElementBase::ElementSize(
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

FloatSize ImageElementBase::DefaultDestinationSize(
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

bool ImageElementBase::IsAccelerated() const {
  return false;
}

const KURL& ImageElementBase::SourceURL() const {
  return CachedImage()->GetResponse().Url();
}

int ImageElementBase::SourceWidth() {
  SourceImageStatus status;
  RefPtr<Image> image = GetSourceImageForCanvas(&status, kPreferNoAcceleration,
                                                kSnapshotReasonUnknown,
                                                SourceDefaultObjectSize());
  return image->width();
}

int ImageElementBase::SourceHeight() {
  SourceImageStatus status;
  RefPtr<Image> image = GetSourceImageForCanvas(&status, kPreferNoAcceleration,
                                                kSnapshotReasonUnknown,
                                                SourceDefaultObjectSize());
  return image->height();
}

bool ImageElementBase::IsOpaque() const {
  Image* image = const_cast<Element&>(GetElement()).ImageContents();
  return image && image->CurrentFrameKnownToBeOpaque();
}

IntSize ImageElementBase::BitmapSourceSize() const {
  ImageResourceContent* image = CachedImage();
  if (!image)
    return IntSize();
  LayoutSize lSize =
      image->ImageSize(LayoutObject::ShouldRespectImageOrientation(
                           GetElement().GetLayoutObject()),
                       1.0f);
  DCHECK(lSize.Fraction().IsZero());
  return IntSize(lSize.Width().ToInt(), lSize.Height().ToInt());
}

ScriptPromise ImageElementBase::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options,
    ExceptionState& exception_state) {
  DCHECK(event_target.ToLocalDOMWindow());
  if ((crop_rect &&
       !ImageBitmap::IsSourceSizeValid(crop_rect->Width(), crop_rect->Height(),
                                       exception_state)) ||
      !ImageBitmap::IsSourceSizeValid(BitmapSourceSize().Width(),
                                      BitmapSourceSize().Height(),
                                      exception_state))
    return ScriptPromise();
  if (!ImageBitmap::IsResizeOptionValid(options, exception_state))
    return ScriptPromise();
  if (IsSVGSource()) {
    return ImageBitmap::CreateAsync(this, crop_rect,
                                    event_target.ToLocalDOMWindow()->document(),
                                    script_state, options);
  }
  return ImageBitmapSource::FulfillImageBitmap(
      script_state, ImageBitmap::Create(
                        this, crop_rect,
                        event_target.ToLocalDOMWindow()->document(), options));
}

}  // namespace blink
