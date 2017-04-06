// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagebitmap/ImageBitmapRenderingContext.h"

#include "bindings/modules/v8/RenderingContext.h"
#include "core/frame/ImageBitmap.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/gpu/ImageLayerBridge.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

ImageBitmapRenderingContext::ImageBitmapRenderingContext(
    HTMLCanvasElement* canvas,
    const CanvasContextCreationAttributes& attrs,
    Document& document)
    : CanvasRenderingContext(canvas, nullptr, attrs),
      m_imageLayerBridge(
          new ImageLayerBridge(attrs.alpha() ? NonOpaque : Opaque)) {}

ImageBitmapRenderingContext::~ImageBitmapRenderingContext() {}

void ImageBitmapRenderingContext::setCanvasGetContextResult(
    RenderingContext& result) {
  result.setImageBitmapRenderingContext(this);
}

void ImageBitmapRenderingContext::transferFromImageBitmap(
    ImageBitmap* imageBitmap,
    ExceptionState& exceptionState) {
  if (imageBitmap && imageBitmap->isNeutered()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The input ImageBitmap has been detached");
    return;
  }

  m_imageLayerBridge->setImage(imageBitmap ? imageBitmap->bitmapImage()
                                           : nullptr);

  didDraw();

  if (imageBitmap)
    imageBitmap->close();
}

CanvasRenderingContext* ImageBitmapRenderingContext::Factory::create(
    HTMLCanvasElement* canvas,
    const CanvasContextCreationAttributes& attrs,
    Document& document) {
  if (!RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled())
    return nullptr;
  return new ImageBitmapRenderingContext(canvas, attrs, document);
}

void ImageBitmapRenderingContext::stop() {
  m_imageLayerBridge->dispose();
}

PassRefPtr<Image> ImageBitmapRenderingContext::getImage(AccelerationHint,
                                                        SnapshotReason) const {
  return m_imageLayerBridge->image();
}

WebLayer* ImageBitmapRenderingContext::platformLayer() const {
  return m_imageLayerBridge->platformLayer();
}

bool ImageBitmapRenderingContext::isPaintable() const {
  return !!m_imageLayerBridge->image();
}

DEFINE_TRACE(ImageBitmapRenderingContext) {
  visitor->trace(m_imageLayerBridge);
  CanvasRenderingContext::trace(visitor);
}

bool ImageBitmapRenderingContext::isAccelerated() const {
  return m_imageLayerBridge->isAccelerated();
}

}  // blink
