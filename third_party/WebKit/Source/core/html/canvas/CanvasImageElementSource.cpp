// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasImageElementSource.h"

#include "core/layout/LayoutObject.h"
#include "core/loader/ImageLoader.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

ImageResourceContent* CanvasImageElementSource::cachedImage() const {
  return imageLoader().image();
}

const Element& CanvasImageElementSource::element() const {
  return *imageLoader().element();
}

bool CanvasImageElementSource::isSVGSource() const {
  return cachedImage() && cachedImage()->getImage()->isSVGImage();
}

PassRefPtr<Image> CanvasImageElementSource::getSourceImageForCanvas(
    SourceImageStatus* status,
    AccelerationHint,
    SnapshotReason,
    const FloatSize& defaultObjectSize) const {
  if (!imageLoader().imageComplete() || !cachedImage()) {
    *status = IncompleteSourceImageStatus;
    return nullptr;
  }

  if (cachedImage()->errorOccurred()) {
    *status = UndecodableSourceImageStatus;
    return nullptr;
  }

  RefPtr<Image> sourceImage;
  if (cachedImage()->getImage()->isSVGImage()) {
    UseCounter::count(element().document(), UseCounter::SVGInCanvas2D);
    SVGImage* svgImage = toSVGImage(cachedImage()->getImage());
    IntSize imageSize =
        roundedIntSize(svgImage->concreteObjectSize(defaultObjectSize));
    sourceImage = SVGImageForContainer::create(
        svgImage, imageSize, 1,
        element().document().completeURL(element().imageSourceURL()));
  } else {
    sourceImage = cachedImage()->getImage();
  }

  *status = NormalSourceImageStatus;
  return sourceImage->imageForDefaultFrame();
}

bool CanvasImageElementSource::wouldTaintOrigin(
    SecurityOrigin* destinationSecurityOrigin) const {
  return cachedImage() &&
         !cachedImage()->isAccessAllowed(destinationSecurityOrigin);
}

FloatSize CanvasImageElementSource::elementSize(
    const FloatSize& defaultObjectSize) const {
  ImageResourceContent* image = cachedImage();
  if (!image)
    return FloatSize();

  if (image->getImage() && image->getImage()->isSVGImage()) {
    return toSVGImage(cachedImage()->getImage())
        ->concreteObjectSize(defaultObjectSize);
  }

  return FloatSize(image->imageSize(
      LayoutObject::shouldRespectImageOrientation(element().layoutObject()),
      1.0f));
}

FloatSize CanvasImageElementSource::defaultDestinationSize(
    const FloatSize& defaultObjectSize) const {
  ImageResourceContent* image = cachedImage();
  if (!image)
    return FloatSize();

  if (image->getImage() && image->getImage()->isSVGImage()) {
    return toSVGImage(cachedImage()->getImage())
        ->concreteObjectSize(defaultObjectSize);
  }

  LayoutSize size;
  size = image->imageSize(
      LayoutObject::shouldRespectImageOrientation(element().layoutObject()),
      1.0f);
  return FloatSize(size);
}

bool CanvasImageElementSource::isAccelerated() const {
  return false;
}

const KURL& CanvasImageElementSource::sourceURL() const {
  return cachedImage()->response().url();
}

int CanvasImageElementSource::sourceWidth() {
  SourceImageStatus status;
  RefPtr<Image> image =
      getSourceImageForCanvas(&status, PreferNoAcceleration,
                              SnapshotReasonUnknown, sourceDefaultObjectSize());
  return image->width();
}

int CanvasImageElementSource::sourceHeight() {
  SourceImageStatus status;
  RefPtr<Image> image =
      getSourceImageForCanvas(&status, PreferNoAcceleration,
                              SnapshotReasonUnknown, sourceDefaultObjectSize());
  return image->height();
}

bool CanvasImageElementSource::isOpaque() const {
  Image* image = const_cast<Element&>(element()).imageContents();
  return image && image->currentFrameKnownToBeOpaque();
}

}  // namespace blink
