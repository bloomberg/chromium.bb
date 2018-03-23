// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/imagebitmap/ImageBitmapRenderingContextBase.h"

#include "bindings/modules/v8/rendering_context.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/ImageLayerBridge.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

ImageBitmapRenderingContextBase::ImageBitmapRenderingContextBase(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs)
    : CanvasRenderingContext(host, attrs),
      image_layer_bridge_(
          new ImageLayerBridge(attrs.alpha ? kNonOpaque : kOpaque)) {}

ImageBitmapRenderingContextBase::~ImageBitmapRenderingContextBase() = default;

void ImageBitmapRenderingContextBase::Stop() {
  image_layer_bridge_->Dispose();
}

void ImageBitmapRenderingContextBase::SetImage(ImageBitmap* image_bitmap) {
  DCHECK(!image_bitmap || !image_bitmap->IsNeutered());

  image_layer_bridge_->SetImage(image_bitmap ? image_bitmap->BitmapImage()
                                             : nullptr);

  DidDraw();

  if (image_bitmap)
    image_bitmap->close();
}

scoped_refptr<StaticBitmapImage> ImageBitmapRenderingContextBase::GetImage(
    AccelerationHint) const {
  return image_layer_bridge_->GetImage();
}

void ImageBitmapRenderingContextBase::SetUV(const FloatPoint left_top,
                                            const FloatPoint right_bottom) {
  image_layer_bridge_->SetUV(left_top, right_bottom);
}

WebLayer* ImageBitmapRenderingContextBase::PlatformLayer() const {
  return image_layer_bridge_->PlatformLayer();
}

bool ImageBitmapRenderingContextBase::IsPaintable() const {
  return !!image_layer_bridge_->GetImage();
}

void ImageBitmapRenderingContextBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_layer_bridge_);
  CanvasRenderingContext::Trace(visitor);
}

bool ImageBitmapRenderingContextBase::IsAccelerated() const {
  return image_layer_bridge_->IsAccelerated();
}

}  // namespace blink
