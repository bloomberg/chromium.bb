// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebImageLayerImpl.h"

#include "ImageLayerChromium.h"
#include "WebLayerImpl.h"

using WebCore::ImageLayerChromium;

namespace WebKit {

WebImageLayer* WebImageLayer::create()
{
    return new WebImageLayerImpl(WebCore::ImageLayerChromium::create());
}

WebImageLayerImpl::WebImageLayerImpl(PassRefPtr<WebCore::ImageLayerChromium> layer)
    : m_layer(adoptPtr(new WebLayerImpl(layer)))
{
}

WebImageLayerImpl::~WebImageLayerImpl()
{
}

WebLayer* WebImageLayerImpl::layer()
{
    return m_layer.get();
}

void WebImageLayerImpl::setBitmap(SkBitmap bitmap)
{
    static_cast<ImageLayerChromium*>(m_layer->layer())->setBitmap(bitmap);
}

} // namespace WebKit
