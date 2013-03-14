// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_content_layer_impl.h"

#include "base/command_line.h"
#include "cc/content_layer.h"
#include "cc/picture_layer.h"
#include "cc/switches.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

using namespace cc;

namespace WebKit {

static bool usingPictureLayer() {
  return cc::switches::IsImplSidePaintingEnabled();
}

WebContentLayerImpl::WebContentLayerImpl(WebContentLayerClient* client)
    : client_(client) {
  if (usingPictureLayer())
    layer_ = make_scoped_ptr(new WebLayerImpl(PictureLayer::Create(this)));
  else
    layer_ = make_scoped_ptr(new WebLayerImpl(ContentLayer::Create(this)));
  layer_->layer()->SetIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl() {
  if (usingPictureLayer())
    static_cast<PictureLayer*>(layer_->layer())->ClearClient();
  else
    static_cast<ContentLayer*>(layer_->layer())->ClearClient();
}

WebLayer* WebContentLayerImpl::layer() { return layer_.get(); }

void WebContentLayerImpl::setDoubleSided(bool double_sided) {
  layer_->layer()->SetDoubleSided(double_sided);
}

void WebContentLayerImpl::setBoundsContainPageScale(
    bool bounds_contain_page_scale) {
  return layer_->layer()->SetBoundsContainPageScale(bounds_contain_page_scale);
}

bool WebContentLayerImpl::boundsContainPageScale() const {
  return layer_->layer()->bounds_contain_page_scale();
}

void WebContentLayerImpl::setAutomaticallyComputeRasterScale(bool automatic) {
  layer_->layer()->SetAutomaticallyComputeRasterScale(automatic);
}

// TODO(alokp): Remove this function from WebContentLayer API.
void WebContentLayerImpl::setUseLCDText(bool enable) {}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable) {
  layer_->layer()->SetDrawCheckerboardForMissingTiles(enable);
}

void WebContentLayerImpl::PaintContents(SkCanvas* canvas,
                                        gfx::Rect clip,
                                        gfx::RectF* opaque) {
  if (!client_)
    return;

  WebFloatRect web_opaque;
  client_->paintContents(
      canvas, clip, layer_->layer()->can_use_lcd_text(), web_opaque);
  *opaque = web_opaque;
}

}  // namespace WebKit
