// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/dom/Document.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "platform/graphics/Image.h"
#include "public/platform/InterfaceProvider.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "wtf/CheckedNumeric.h"

namespace blink {

namespace {

static CanvasImageSource* toImageSourceInternal(
    const CanvasImageSourceUnion& value) {
  if (value.isHTMLImageElement())
    return value.getAsHTMLImageElement();

  if (value.isImageBitmap()) {
    if (static_cast<ImageBitmap*>(value.getAsImageBitmap())->isNeutered())
      return nullptr;
    return value.getAsImageBitmap();
  }

  return nullptr;
}

}  // anonymous namespace

FaceDetector* FaceDetector::create(ScriptState* scriptState) {
  return new FaceDetector(*scriptState->domWindow()->frame());
}

FaceDetector::FaceDetector(LocalFrame& frame) {
  DCHECK(!m_service.is_bound());
  DCHECK(frame.interfaceProvider());
  frame.interfaceProvider()->getInterface(mojo::GetProxy(&m_service));
}

ScriptPromise FaceDetector::detect(ScriptState* scriptState,
                                   const CanvasImageSourceUnion& imageSource) {
  CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!imageSourceInternal) {
    // TODO(mcasas): Implement more CanvasImageSources, https://crbug.com/659138
    NOTIMPLEMENTED() << "Unsupported CanvasImageSource";
    resolver->reject(
        DOMException::create(NotFoundError, "Unsupported source."));
    return promise;
  }

  if (imageSourceInternal->wouldTaintOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin())) {
    resolver->reject(DOMException::create(SecurityError,
                                          "Image source would taint origin."));
    return promise;
  }

  if (imageSource.isHTMLImageElement()) {
    return detectFacesOnImageElement(
        resolver, static_cast<HTMLImageElement*>(imageSourceInternal));
  }
  if (imageSourceInternal->isImageBitmap()) {
    return detectFacesOnImageBitmap(
        resolver, static_cast<ImageBitmap*>(imageSourceInternal));
  }
  NOTREACHED();
  return promise;
}

ScriptPromise FaceDetector::detectFacesOnImageElement(
    ScriptPromiseResolver* resolver,
    const HTMLImageElement* img) {
  ScriptPromise promise = resolver->promise();
  if (img->bitmapSourceSize().isZero()) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  ImageResource* const imageResource = img->cachedImage();
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

  const sk_sp<SkImage> image = blinkImage->imageForCurrentFrame();
  DCHECK_EQ(img->naturalWidth(), image->width());
  DCHECK_EQ(img->naturalHeight(), image->height());

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

  if (!m_service) {
    resolver->reject(DOMException::create(
        NotSupportedError, "Face detection service unavailable."));
    return promise;
  }

  m_serviceRequests.add(resolver);
  DCHECK(m_service.is_bound());
  m_service->DetectFace(std::move(sharedBufferHandle), img->naturalWidth(),
                        img->naturalHeight(),
                        convertToBaseCallback(WTF::bind(
                            &FaceDetector::onDetectFace, wrapPersistent(this),
                            wrapPersistent(resolver))));
  return promise;
}

ScriptPromise FaceDetector::detectFacesOnImageBitmap(
    ScriptPromiseResolver* resolver,
    ImageBitmap* imageBitmap) {
  ScriptPromise promise = resolver->promise();
  if (!imageBitmap->originClean()) {
    resolver->reject(
        DOMException::create(SecurityError, "ImageBitmap is not origin clean"));
    return promise;
  }

  if (imageBitmap->size().area() == 0) {
    resolver->resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  SkPixmap pixmap;
  RefPtr<Uint8Array> pixelData;
  uint8_t* pixelDataPtr = nullptr;
  WTF::CheckedNumeric<int> allocationSize = 0;
  // Use |skImage|'s pixels if it has direct access to them, otherwise retrieve
  // them from elsewhere via copyBitmapData().
  sk_sp<SkImage> skImage = imageBitmap->bitmapImage()->imageForCurrentFrame();
  if (skImage->peekPixels(&pixmap)) {
    pixelDataPtr = static_cast<uint8_t*>(pixmap.writable_addr());
    allocationSize = pixmap.getSafeSize();
  } else {
    pixelData = imageBitmap->copyBitmapData(imageBitmap->isPremultiplied()
                                                ? PremultiplyAlpha
                                                : DontPremultiplyAlpha,
                                            N32ColorType);
    pixelDataPtr = pixelData->data();
    allocationSize = imageBitmap->size().area() * 4 /* bytes per pixel */;
  }

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      mojo::SharedBufferHandle::Create(allocationSize.ValueOrDefault(0));

  if (!pixelDataPtr || !sharedBufferHandle->is_valid()) {
    resolver->reject(
        DOMException::create(InvalidStateError, "Internal allocation error"));
    return promise;
  }

  if (!m_service) {
    resolver->reject(DOMException::create(
        NotFoundError, "Face detection service unavailable."));
    return promise;
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(allocationSize.ValueOrDefault(0));
  DCHECK(mappedBuffer.get());

  memcpy(mappedBuffer.get(), pixelDataPtr, allocationSize.ValueOrDefault(0));

  m_serviceRequests.add(resolver);
  DCHECK(m_service.is_bound());
  m_service->DetectFace(std::move(sharedBufferHandle), imageBitmap->width(),
                        imageBitmap->height(),
                        convertToBaseCallback(WTF::bind(
                            &FaceDetector::onDetectFace, wrapPersistent(this),
                            wrapPersistent(resolver))));
  sharedBufferHandle.reset();
  return promise;
}

void FaceDetector::onDetectFace(
    ScriptPromiseResolver* resolver,
    mojom::blink::FaceDetectionResultPtr faceDetectionResult) {
  if (!m_serviceRequests.contains(resolver))
    return;

  HeapVector<Member<DOMRect>> detectedFaces;
  for (const auto& boundingBox : faceDetectionResult->boundingBoxes) {
    detectedFaces.append(DOMRect::create(boundingBox->x, boundingBox->y,
                                         boundingBox->width,
                                         boundingBox->height));
  }

  resolver->resolve(detectedFaces);
  m_serviceRequests.remove(resolver);
}

DEFINE_TRACE(FaceDetector) {
  visitor->trace(m_serviceRequests);
}

}  // namespace blink
