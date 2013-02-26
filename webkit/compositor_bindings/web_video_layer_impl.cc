// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_video_layer_impl.h"

#include "base/bind.h"
#include "cc/video_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_to_ccvideo_frame_provider.h"
#include "webkit/media/webvideoframe_impl.h"

namespace WebKit {

WebVideoLayerImpl::WebVideoLayerImpl(WebVideoFrameProvider* web_provider)
    : provider_adapter_(
          webkit::WebToCCVideoFrameProvider::Create(web_provider)),
      layer_(
          new WebLayerImpl(cc::VideoLayer::create(provider_adapter_.get()))) {}

WebVideoLayerImpl::~WebVideoLayerImpl() {}

WebLayer* WebVideoLayerImpl::layer() { return layer_.get(); }

bool WebVideoLayerImpl::active() const {
  return layer_->layer()->layerTreeHost();
}

}  // namespace WebKit
