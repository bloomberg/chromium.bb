// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/DOMRect.h"
#include "core/dom/Document.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "platform/graphics/Image.h"
#include "public/platform/InterfaceProvider.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

namespace {

mojo::ScopedSharedBufferHandle getSharedBufferHandle(
    const HTMLImageElement* img,
    ScriptPromiseResolver* resolver) {
  ImageResource* const imageResource = img->cachedImage();
  // TODO(xianglu): Add test case for undecodable images.
  if (!imageResource || imageResource->errorOccurred()) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to load or decode HTMLImageElement."));
    return mojo::ScopedSharedBufferHandle();
  }

  Image* const blinkImage = imageResource->getImage();
  if (!blinkImage) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get image from resource."));
    return mojo::ScopedSharedBufferHandle();
  }

  const sk_sp<SkImage> image = blinkImage->imageForCurrentFrame();
  DCHECK_EQ(img->naturalWidth(), image->width());
  DCHECK_EQ(img->naturalHeight(), image->height());

  if (!image) {
    resolver->reject(DOMException::create(
        InvalidStateError, "Failed to get image from current frame."));
    return mojo::ScopedSharedBufferHandle();
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
    // Also, add layouttest for this case later.
    resolver->reject(
        DOMException::create(InvalidStateError, "Image exceeds size limit."));
    return mojo::ScopedSharedBufferHandle();
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(allocationSize);

  const SkPixmap pixmap(skiaInfo, mappedBuffer.get(), skiaInfo.minRowBytes());
  if (!image->readPixels(pixmap, 0, 0)) {
    resolver->reject(DOMException::create(
        InvalidStateError,
        "Failed to read pixels: Unable to decompress or unsupported format."));
    return mojo::ScopedSharedBufferHandle();
  }

  return sharedBufferHandle;
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
                                   const HTMLImageElement* img) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // TODO(xianglu): Add test cases for cross-origin-images.
  if (img->wouldTaintOrigin(
          scriptState->getExecutionContext()->getSecurityOrigin())) {
    resolver->reject(DOMException::create(
        SecurityError, "Image source from a different origin."));
    return promise;
  }

  if (img->bitmapSourceSize().isZero()) {
    resolver->reject(
        DOMException::create(InvalidStateError, "HTMLImageElement is empty."));
    return promise;
  }

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      getSharedBufferHandle(img, resolver);
  if (!sharedBufferHandle->is_valid())
    return promise;

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
