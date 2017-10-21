// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagebitmap/ImageBitmapRenderingContext.h"

#include "bindings/modules/v8/rendering_context.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/ImageLayerBridge.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributes& attrs)
    : CanvasRenderingContext(host, attrs),
      image_layer_bridge_(
          new ImageLayerBridge(attrs.alpha() ? kNonOpaque : kOpaque)) {}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() {}

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

  image_layer_bridge_->SetImage(image_bitmap ? image_bitmap->BitmapImage()
                                             : nullptr);

  DidDraw();

  if (image_bitmap)
    image_bitmap->close();
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributes& attrs) {
  if (!RuntimeEnabledFeatures::ExperimentalCanvasFeaturesEnabled())
    return nullptr;
  return new ImageBitmapRenderingContext(host, attrs);
}

void ImageBitmapRenderingContext::Stop() {
  image_layer_bridge_->Dispose();
}

scoped_refptr<StaticBitmapImage> ImageBitmapRenderingContext::GetImage(
    AccelerationHint,
    SnapshotReason) const {
  return image_layer_bridge_->GetImage();
}

WebLayer* ImageBitmapRenderingContext::PlatformLayer() const {
  return image_layer_bridge_->PlatformLayer();
}

bool ImageBitmapRenderingContext::IsPaintable() const {
  return !!image_layer_bridge_->GetImage();
}

void ImageBitmapRenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_layer_bridge_);
  CanvasRenderingContext::Trace(visitor);
}

bool ImageBitmapRenderingContext::IsAccelerated() const {
  return image_layer_bridge_->IsAccelerated();
}

}  // blink
