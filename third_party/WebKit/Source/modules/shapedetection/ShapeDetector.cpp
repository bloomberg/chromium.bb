// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/ShapeDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/dom/Document.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "platform/graphics/Image.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "wtf/CheckedNumeric.h"

namespace blink {

namespace {

mojo::ScopedSharedBufferHandle getSharedBufferOnData(
    ScriptPromiseResolver* resolver,
    uint8_t* data,
    int size) {
  DCHECK(data);
  DCHECK(size);
  ScriptPromise promise = resolver->promise();

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      mojo::SharedBufferHandle::Create(size);
  if (!sharedBufferHandle->is_valid()) {
    resolver->reject(
        DOMException::create(InvalidStateError, "Internal allocation error"));
    return sharedBufferHandle;
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(size);
  DCHECK(mappedBuffer.get());
  memcpy(mappedBuffer.get(), data, size);

  return sharedBufferHandle;
}

}  // anonymous namespace

ScriptPromise ShapeDetector::detect(ScriptState* scriptState,
                                    const ImageBitmapSourceUnion& imageSource) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // ImageDatas cannot be tainted by definition.
  if (imageSource.isImageData())
    return detectShapesOnImageData(resolver, imageSource.getAsImageData());

  CanvasImageSource* canvasImageSource;
  if (imageSource.isHTMLImageElement()) {
    canvasImageSource = imageSource.getAsHTMLImageElement();
  } else if (imageSource.isImageBitmap()) {
    canvasImageSource = imageSource.getAsImageBitmap();
  } else if (imageSource.isHTMLVideoElement()) {
    canvasImageSource = imageSource.getAsHTMLVideoElement();
  } else if (imageSource.isHTMLCanvasElement()) {
    canvasImageSource = imageSource.getAsHTMLCanvasElement();
  } else if (imageSource.isOffscreenCanvas()) {
    canvasImageSource = imageSource.getAsOffscreenCanvas();
  } else {
    NOTREACHED() << "Unsupported CanvasImageSource";
    resolver->reject(
        DOMException::create(NotSupportedError, "Unsupported source."));
    return promise;
  }

  if (canvasImageSource->wouldTaintOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin())) {
    resolver->reject(
        DOMException::create(SecurityError, "Source would taint origin."));
    return promise;
  }

  if (imageSource.isHTMLImageElement()) {
    return detectShapesOnImageElement(resolver,
                                      imageSource.getAsHTMLImageElement());
  }

  // TODO(mcasas): Check if |video| is actually playing a MediaStream by using
  // HTMLMediaElement::isMediaStreamURL(video->currentSrc().getString()); if
  // there is a local WebCam associated, there might be sophisticated ways to
  // detect faces on it. Until then, treat as a normal <video> element.

  const FloatSize size(canvasImageSource->sourceWidth(),
                       canvasImageSource->sourceHeight());

  SourceImageStatus sourceImageStatus = InvalidSourceImageStatus;
  RefPtr<Image> image = canvasImageSource->getSourceImageForCanvas(
      &sourceImageStatus, PreferNoAcceleration, SnapshotReasonDrawImage, size);
  if (!image || sourceImageStatus != NormalSourceImageStatus) {
    resolver->reject(
        DOMException::create(InvalidStateError, "Invalid element or state."));
    return promise;
  }
  if (size.isEmpty()) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  SkPixmap pixmap;
  RefPtr<Uint8Array> pixelData;
  uint8_t* pixelDataPtr = nullptr;
  WTF::CheckedNumeric<int> allocationSize = 0;

  // TODO(ccameron): ShapeDetector can ignore color conversion.
  sk_sp<SkImage> skImage =
      image->imageForCurrentFrame(ColorBehavior::transformToGlobalTarget());
  // Use |skImage|'s pixels if it has direct access to them.
  if (skImage->peekPixels(&pixmap)) {
    pixelDataPtr = static_cast<uint8_t*>(pixmap.writable_addr());
    allocationSize = pixmap.getSafeSize();
  } else if (imageSource.isImageBitmap()) {
    ImageBitmap* imageBitmap = imageSource.getAsImageBitmap();
    pixelData = imageBitmap->copyBitmapData(imageBitmap->isPremultiplied()
                                                ? PremultiplyAlpha
                                                : DontPremultiplyAlpha,
                                            N32ColorType);
    pixelDataPtr = pixelData->data();
    allocationSize = imageBitmap->size().area() * 4 /* bytes per pixel */;
  } else {
    // TODO(mcasas): retrieve the pixels from elsewhere.
    NOTREACHED();
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get pixels for current frame."));
    return promise;
  }

  mojo::ScopedSharedBufferHandle sharedBufferHandle = getSharedBufferOnData(
      resolver, pixelDataPtr, allocationSize.ValueOrDefault(0));
  if (!sharedBufferHandle->is_valid())
    return promise;

  return doDetect(resolver, std::move(sharedBufferHandle), image->width(),
                  image->height());
}

ScriptPromise ShapeDetector::detectShapesOnImageData(
    ScriptPromiseResolver* resolver,
    ImageData* imageData) {
  ScriptPromise promise = resolver->promise();

  if (imageData->size().isZero()) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  uint8_t* const data = imageData->data()->data();
  WTF::CheckedNumeric<int> allocationSize = imageData->size().area() * 4;

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      getSharedBufferOnData(resolver, data, allocationSize.ValueOrDefault(0));
  if (!sharedBufferHandle->is_valid())
    return promise;

  return doDetect(resolver, std::move(sharedBufferHandle), imageData->width(),
                  imageData->height());
}

ScriptPromise ShapeDetector::detectShapesOnImageElement(
    ScriptPromiseResolver* resolver,
    const HTMLImageElement* img) {
  ScriptPromise promise = resolver->promise();

  if (img->bitmapSourceSize().isZero()) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  ImageResourceContent* const imageResource = img->cachedImage();
  if (!imageResource || imageResource->errorOccurred()) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to load or decode HTMLImageElement."));
    return promise;
  }

  Image* const blinkImage = imageResource->getImage();
  if (!blinkImage) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get image from resource."));
    return promise;
  }

  // TODO(ccameron): ShapeDetector can ignore color conversion.
  const sk_sp<SkImage> image = blinkImage->imageForCurrentFrame(
      ColorBehavior::transformToGlobalTarget());
  DCHECK_EQ(img->naturalWidth(), static_cast<unsigned>(image->width()));
  DCHECK_EQ(img->naturalHeight(), static_cast<unsigned>(image->height()));

  if (!image) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get image from current frame."));
    return promise;
  }

  const SkImageInfo skiaInfo =
      SkImageInfo::MakeN32(image->width(), image->height(), image->alphaType());

  const uint32_t allocationSize = skiaInfo.getSafeSize(skiaInfo.minRowBytes());

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      mojo::SharedBufferHandle::Create(allocationSize);
  if (!sharedBufferHandle.is_valid()) {
    DLOG(ERROR) << "Requested allocation : " << allocationSize
                << "B, larger than |mojo::edk::kMaxSharedBufferSize| == 16MB ";
    // TODO(xianglu): For now we reject the promise if the image is too large.
    // But consider resizing the image to remove restriction on the user side.
    // Also, add LayoutTests for this case later.
    resolver->reject(
        DOMException::create(InvalidStateError, "Image exceeds size limit."));
    return promise;
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(allocationSize);

  const SkPixmap pixmap(skiaInfo, mappedBuffer.get(), skiaInfo.minRowBytes());
  if (!image->readPixels(pixmap, 0, 0)) {
    resolver->reject(DOMException::create(
        InvalidStateError,
        "Failed to read pixels: Unable to decompress or unsupported format."));
    return promise;
  }

  return doDetect(resolver, std::move(sharedBufferHandle), img->naturalWidth(),
                  img->naturalHeight());
}

}  // namespace blink
