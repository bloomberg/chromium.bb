// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_image_layer_impl.h"

#include "base/command_line.h"
#include "cc/image_layer.h"
#include "cc/picture_image_layer.h"
#include "cc/switches.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

static bool usingPictureLayer()
{
    return CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnableImplSidePainting);
}

namespace WebKit {

WebImageLayerImpl::WebImageLayerImpl()
{
    if (usingPictureLayer())
        m_layer.reset(new WebLayerImpl(cc::PictureImageLayer::create()));
    else
        m_layer.reset(new WebLayerImpl(cc::ImageLayer::create()));
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
    if (usingPictureLayer())
        static_cast<cc::PictureImageLayer*>(m_layer->layer())->setBitmap(bitmap);
    else
        static_cast<cc::ImageLayer*>(m_layer->layer())->setBitmap(bitmap);
}

} // namespace WebKit
