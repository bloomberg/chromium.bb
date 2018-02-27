// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/imagebitmap/ImageBitmapRenderingContext.h"

#include "bindings/modules/v8/rendering_context.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "platform/graphics/StaticBitmapImage.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : ImageBitmapRenderingContextBase(host, attrs) {}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() = default;

void ImageBitmapRenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetImageBitmapRenderingContext(this);
}

void ImageBitmapRenderingContext::transferFromImageBitmap(
    ImageBitmap* image_bitmap,
    ExceptionState& exception_state) {
  if (image_bitmap && image_bitmap->IsNeutered()) {
    exception_state.ThrowDOMException(
        kInvalidStateError, "The input ImageBitmap has been detached");
    return;
  }

  SetImage(image_bitmap);
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  return new ImageBitmapRenderingContext(host, attrs);
}

}  // namespace blink
