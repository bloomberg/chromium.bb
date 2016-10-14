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
    const HTMLImageElement* img) {
  ImageResource* const imageResource = img->cachedImage();
  if (!imageResource) {
    DLOG(ERROR) << "Failed to convert HTMLImageElement to ImageSource.";
    return mojo::ScopedSharedBufferHandle();
  }

  Image* const blinkImage = imageResource->getImage();
  if (!blinkImage) {
    DLOG(ERROR) << "Failed to convert ImageSource to blink::Image.";
    return mojo::ScopedSharedBufferHandle();
  }

  const sk_sp<SkImage> image = blinkImage->imageForCurrentFrame();
  DCHECK_EQ(img->naturalWidth(), image->width());
  DCHECK_EQ(img->naturalHeight(), image->height());

  if (!image) {
    DLOG(ERROR) << "Failed to convert blink::Image to sk_sp<SkImage>.";
    return mojo::ScopedSharedBufferHandle();
  }

  const SkImageInfo skiaInfo =
      SkImageInfo::MakeN32(image->width(), image->height(), image->alphaType());

  const uint32_t allocationSize = skiaInfo.getSafeSize(skiaInfo.minRowBytes());

  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      mojo::SharedBufferHandle::Create(allocationSize);
  if (!sharedBufferHandle.is_valid()) {
    // TODO(xianglu): Do something when the image is too large.
    DLOG(ERROR) << "Failed to create a sharedBufferHandle. allocationSize = "
                << allocationSize << "bytes. limit = 16777216";
    return mojo::ScopedSharedBufferHandle();
  }

  const mojo::ScopedSharedBufferMapping mappedBuffer =
      sharedBufferHandle->Map(allocationSize);

  const SkPixmap pixmap(skiaInfo, mappedBuffer.get(), skiaInfo.minRowBytes());
  if (!image->readPixels(pixmap, 0, 0)) {
    DLOG(ERROR) << "Failed to read pixels from sk_sp<SkImage>.";
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

  if (!m_service) {
    resolver->reject(DOMException::create(
        NotFoundError, "Face detection service unavailable."));
    return promise;
  }

  if (!img) {
    resolver->reject(DOMException::create(
        SyntaxError, "The provided HTMLImageElement is empty."));
    return promise;
  }

  // TODO(xianglu): Add security check when the spec is ready.
  // https://crbug.com/646083
  mojo::ScopedSharedBufferHandle sharedBufferHandle =
      getSharedBufferHandle(img);
  if (!sharedBufferHandle->is_valid()) {
    resolver->reject(DOMException::create(
        SyntaxError, "Request for sharedBufferHandle failed."));
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
