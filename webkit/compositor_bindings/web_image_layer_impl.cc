// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_image_layer_impl.h"

#include "cc/image_layer.h"
#include "web_layer_impl.h"

using cc::ImageLayer;

namespace WebKit {

WebImageLayer* WebImageLayer::create()
{
    return new WebImageLayerImpl();
}

WebImageLayerImpl::WebImageLayerImpl()
    : m_layer(new WebLayerImpl(ImageLayer::create()))
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
    static_cast<ImageLayer*>(m_layer->layer())->setBitmap(bitmap);
}

} // namespace WebKit
