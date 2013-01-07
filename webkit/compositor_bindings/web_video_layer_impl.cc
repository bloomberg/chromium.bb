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
    : m_providerAdapter(webkit::WebToCCVideoFrameProvider::Create(web_provider))
    , m_layer(new WebLayerImpl(cc::VideoLayer::create(m_providerAdapter.get())))
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
