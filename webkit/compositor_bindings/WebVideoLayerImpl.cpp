// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebVideoLayerImpl.h"

#include "VideoLayerChromium.h"
#include "WebLayerImpl.h"

namespace WebKit {

WebVideoLayer* WebVideoLayer::create(WebVideoFrameProvider* provider)
{
    return new WebVideoLayerImpl(provider);
}

WebVideoLayerImpl::WebVideoLayerImpl(WebVideoFrameProvider* provider)
    : m_layer(adoptPtr(new WebLayerImpl(cc::VideoLayerChromium::create(provider))))
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
