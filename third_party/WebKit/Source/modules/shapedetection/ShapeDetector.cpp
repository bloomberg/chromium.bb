// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/ShapeDetector.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/geometry/DOMRect.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/ImageData.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "platform/graphics/Image.h"
#include "platform/wtf/CheckedNumeric.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

namespace {

skia::mojom::blink::BitmapPtr createBitmapFromData(int width,
                                                   int height,
                                                   Vector<uint8_t> bitmapData) {
  skia::mojom::blink::BitmapPtr bitmap = skia::mojom::blink::Bitmap::New();

  bitmap->color_type = (kN32_SkColorType == kRGBA_8888_SkColorType)
                           ? skia::mojom::blink::ColorType::RGBA_8888
                           : skia::mojom::blink::ColorType::BGRA_8888;
  bitmap->alpha_type = skia::mojom::blink::AlphaType::ALPHA_TYPE_OPAQUE;
  bitmap->profile_type = skia::mojom::blink::ColorProfileType::LINEAR;
  bitmap->width = width;
  bitmap->height = height;
  bitmap->row_bytes = width * 4 /* bytes per pixel */;
  bitmap->pixel_data = std::move(bitmapData);

  return bitmap;
}

}  // anonymous namespace

ScriptPromise ShapeDetector::detect(
    ScriptState* script_state,
    const ImageBitmapSourceUnion& image_source) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // ImageDatas cannot be tainted by definition.
  if (image_source.IsImageData())
    return DetectShapesOnImageData(resolver, image_source.GetAsImageData());

  CanvasImageSource* canvas_image_source;
  if (image_source.IsHTMLImageElement()) {
    canvas_image_source = image_source.GetAsHTMLImageElement();
  } else if (image_source.IsImageBitmap()) {
    canvas_image_source = image_source.GetAsImageBitmap();
  } else if (image_source.IsHTMLVideoElement()) {
    canvas_image_source = image_source.GetAsHTMLVideoElement();
  } else if (image_source.IsHTMLCanvasElement()) {
    canvas_image_source = image_source.GetAsHTMLCanvasElement();
  } else if (image_source.IsOffscreenCanvas()) {
    canvas_image_source = image_source.GetAsOffscreenCanvas();
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

  if (image_source.IsHTMLImageElement()) {
    return DetectShapesOnImageElement(resolver,
                                      image_source.GetAsHTMLImageElement());
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

  // makeNonTextureImage() will make a raster copy of
  // PaintImageForCurrentFrame() if needed, otherwise returning the original
  // SkImage.
  const sk_sp<SkImage> sk_image =
      image->PaintImageForCurrentFrame().GetSkImage()->makeNonTextureImage();
  if (sk_image && sk_image->peekPixels(&pixmap)) {
    pixel_data_ptr = static_cast<uint8_t*>(pixmap.writable_addr());
    allocation_size = pixmap.computeByteSize();
  } else {
    // TODO(mcasas): retrieve the pixels from elsewhere.
    NOTREACHED();
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Failed to get pixels for current frame."));
    return promise;
  }

  WTF::Vector<uint8_t> bitmap_data;
  bitmap_data.Append(pixel_data_ptr,
                     static_cast<int>(allocation_size.ValueOrDefault(0)));

  return DoDetect(resolver,
                  createBitmapFromData(image->width(), image->height(),
                                       std::move(bitmap_data)));
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

  WTF::Vector<uint8_t> bitmap_data;
  bitmap_data.Append(data, static_cast<int>(allocation_size.ValueOrDefault(0)));

  return DoDetect(
      resolver, createBitmapFromData(image_data->width(), image_data->height(),
                                     std::move(bitmap_data)));
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

  const sk_sp<SkImage> image =
      blink_image->PaintImageForCurrentFrame().GetSkImage();
  DCHECK_EQ(img->naturalWidth(), static_cast<unsigned>(image->width()));
  DCHECK_EQ(img->naturalHeight(), static_cast<unsigned>(image->height()));

  if (!image) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Failed to get image from current frame."));
    return promise;
  }

  const SkImageInfo skia_info =
      SkImageInfo::MakeN32(image->width(), image->height(), image->alphaType());
  size_t rowBytes = skia_info.minRowBytes();

  Vector<uint8_t> bitmap_data(skia_info.computeByteSize(rowBytes));
  const SkPixmap pixmap(skia_info, bitmap_data.data(), rowBytes);

  if (!image->readPixels(pixmap, 0, 0)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "Failed to read pixels: Unable to decompress or unsupported format."));
    return promise;
  }

  return DoDetect(
      resolver, createBitmapFromData(img->naturalWidth(), img->naturalHeight(),
                                     std::move(bitmap_data)));
}

}  // namespace blink
