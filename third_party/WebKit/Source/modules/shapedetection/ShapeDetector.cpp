// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/ShapeDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalFrame.h"
#include "core/geometry/DOMRect.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "platform/graphics/Image.h"
#include "platform/wtf/CheckedNumeric.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

namespace {

mojo::ScopedSharedBufferHandle GetSharedBufferOnData(
    ScriptPromiseResolver* resolver,
    uint8_t* data,
    int size) {
  DCHECK(data);
  DCHECK(size);
  ScriptPromise promise = resolver->Promise();

  mojo::ScopedSharedBufferHandle shared_buffer_handle =
      mojo::SharedBufferHandle::Create(size);
  if (!shared_buffer_handle->is_valid()) {
    resolver->Reject(
        DOMException::Create(kInvalidStateError, "Internal allocation error"));
    return shared_buffer_handle;
  }

  const mojo::ScopedSharedBufferMapping mapped_buffer =
      shared_buffer_handle->Map(size);
  DCHECK(mapped_buffer.get());
  memcpy(mapped_buffer.get(), data, size);

  return shared_buffer_handle;
}

}  // anonymous namespace

ScriptPromise ShapeDetector::detect(
    ScriptState* script_state,
    const ImageBitmapSourceUnion& image_source) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // ImageDatas cannot be tainted by definition.
  if (image_source.isImageData())
    return DetectShapesOnImageData(resolver, image_source.getAsImageData());

  CanvasImageSource* canvas_image_source;
  if (image_source.isHTMLImageElement()) {
    canvas_image_source = image_source.getAsHTMLImageElement();
  } else if (image_source.isImageBitmap()) {
    canvas_image_source = image_source.getAsImageBitmap();
  } else if (image_source.isHTMLVideoElement()) {
    canvas_image_source = image_source.getAsHTMLVideoElement();
  } else if (image_source.isHTMLCanvasElement()) {
    canvas_image_source = image_source.getAsHTMLCanvasElement();
  } else if (image_source.isOffscreenCanvas()) {
    canvas_image_source = image_source.getAsOffscreenCanvas();
  } else {
    NOTREACHED() << "Unsupported CanvasImageSource";
    resolver->Reject(
        DOMException::Create(kNotSupportedError, "Unsupported source."));
    return promise;
  }

  if (canvas_image_source->WouldTaintOrigin(
          ExecutionContext::From(script_state)->GetSecurityOrigin())) {
    resolver->Reject(
        DOMException::Create(kSecurityError, "Source would taint origin."));
    return promise;
  }

  if (image_source.isHTMLImageElement()) {
    return DetectShapesOnImageElement(resolver,
                                      image_source.getAsHTMLImageElement());
  }

  // TODO(mcasas): Check if |video| is actually playing a MediaStream by using
  // HTMLMediaElement::isMediaStreamURL(video->currentSrc().getString()); if
  // there is a local WebCam associated, there might be sophisticated ways to
  // detect faces on it. Until then, treat as a normal <video> element.

  const FloatSize size(canvas_image_source->SourceWidth(),
                       canvas_image_source->SourceHeight());

  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  RefPtr<Image> image = canvas_image_source->GetSourceImageForCanvas(
      &source_image_status, kPreferNoAcceleration, kSnapshotReasonDrawImage,
      size);
  if (!image || source_image_status != kNormalSourceImageStatus) {
    resolver->Reject(
        DOMException::Create(kInvalidStateError, "Invalid element or state."));
    return promise;
  }
  if (size.IsEmpty()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  SkPixmap pixmap;
  RefPtr<Uint8Array> pixel_data;
  uint8_t* pixel_data_ptr = nullptr;
  WTF::CheckedNumeric<int> allocation_size = 0;

  sk_sp<SkImage> sk_image = image->ImageForCurrentFrame();
  // Use |skImage|'s pixels if it has direct access to them.
  if (sk_image->peekPixels(&pixmap)) {
    pixel_data_ptr = static_cast<uint8_t*>(pixmap.writable_addr());
    allocation_size = pixmap.getSafeSize();
  } else if (image_source.isImageBitmap()) {
    ImageBitmap* image_bitmap = image_source.getAsImageBitmap();
    pixel_data = image_bitmap->CopyBitmapData(image_bitmap->IsPremultiplied()
                                                  ? kPremultiplyAlpha
                                                  : kDontPremultiplyAlpha,
                                              kN32ColorType);
    pixel_data_ptr = pixel_data->Data();
    allocation_size = image_bitmap->Size().Area() * 4 /* bytes per pixel */;
  } else {
    // TODO(mcasas): retrieve the pixels from elsewhere.
    NOTREACHED();
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Failed to get pixels for current frame."));
    return promise;
  }

  mojo::ScopedSharedBufferHandle shared_buffer_handle = GetSharedBufferOnData(
      resolver, pixel_data_ptr, allocation_size.ValueOrDefault(0));
  if (!shared_buffer_handle->is_valid())
    return promise;

  return DoDetect(resolver, std::move(shared_buffer_handle), image->width(),
                  image->height());
}

ScriptPromise ShapeDetector::DetectShapesOnImageData(
    ScriptPromiseResolver* resolver,
    ImageData* image_data) {
  ScriptPromise promise = resolver->Promise();

  if (image_data->Size().IsZero()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  uint8_t* const data = image_data->data()->Data();
  WTF::CheckedNumeric<int> allocation_size = image_data->Size().Area() * 4;

  mojo::ScopedSharedBufferHandle shared_buffer_handle =
      GetSharedBufferOnData(resolver, data, allocation_size.ValueOrDefault(0));
  if (!shared_buffer_handle->is_valid())
    return promise;

  return DoDetect(resolver, std::move(shared_buffer_handle),
                  image_data->width(), image_data->height());
}

ScriptPromise ShapeDetector::DetectShapesOnImageElement(
    ScriptPromiseResolver* resolver,
    const HTMLImageElement* img) {
  ScriptPromise promise = resolver->Promise();

  if (img->BitmapSourceSize().IsZero()) {
    resolver->Resolve(HeapVector<Member<DOMRect>>());
    return promise;
  }

  ImageResourceContent* const image_resource = img->CachedImage();
  if (!image_resource || image_resource->ErrorOccurred()) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Failed to load or decode HTMLImageElement."));
    return promise;
  }

  Image* const blink_image = image_resource->GetImage();
  if (!blink_image) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Failed to get image from resource."));
    return promise;
  }

  const sk_sp<SkImage> image = blink_image->ImageForCurrentFrame();
  DCHECK_EQ(img->naturalWidth(), static_cast<unsigned>(image->width()));
  DCHECK_EQ(img->naturalHeight(), static_cast<unsigned>(image->height()));

  if (!image) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Failed to get image from current frame."));
    return promise;
  }

  const SkImageInfo skia_info =
      SkImageInfo::MakeN32(image->width(), image->height(), image->alphaType());

  const uint32_t allocation_size =
      skia_info.getSafeSize(skia_info.minRowBytes());

  mojo::ScopedSharedBufferHandle shared_buffer_handle =
      mojo::SharedBufferHandle::Create(allocation_size);
  if (!shared_buffer_handle.is_valid()) {
    DLOG(ERROR) << "Requested allocation : " << allocation_size
                << "B, larger than |mojo::edk::kMaxSharedBufferSize| == 16MB ";
    // TODO(xianglu): For now we reject the promise if the image is too large.
    // But consider resizing the image to remove restriction on the user side.
    // Also, add LayoutTests for this case later.
    resolver->Reject(
        DOMException::Create(kInvalidStateError, "Image exceeds size limit."));
    return promise;
  }

  const mojo::ScopedSharedBufferMapping mapped_buffer =
      shared_buffer_handle->Map(allocation_size);

  const SkPixmap pixmap(skia_info, mapped_buffer.get(),
                        skia_info.minRowBytes());
  if (!image->readPixels(pixmap, 0, 0)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "Failed to read pixels: Unable to decompress or unsupported format."));
    return promise;
  }

  return DoDetect(resolver, std::move(shared_buffer_handle),
                  img->naturalWidth(), img->naturalHeight());
}

}  // namespace blink
