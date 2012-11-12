// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_video_layer_impl.h"

#include "base/bind.h"
#include "cc/video_layer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/media/webvideoframe_impl.h"

namespace WebKit {

WebVideoLayerImpl::WebVideoLayerImpl(WebVideoFrameProvider* provider)
    : m_layer(new WebLayerImpl(
        cc::VideoLayer::create(
            provider,
            base::Bind(webkit_media::WebVideoFrameImpl::toVideoFrame))))
{
}

WebVideoLayerImpl::~WebVideoLayerImpl()
{
}

WebLayer* WebVideoLayerImpl::layer()
{
    return m_layer.get();
}

bool WebVideoLayerImpl::active() const
{
    return m_layer->layer()->layerTreeHost();
}

} // namespace WebKit
