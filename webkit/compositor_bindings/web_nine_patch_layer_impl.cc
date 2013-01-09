// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_nine_patch_layer_impl.h"

#include "cc/nine_patch_layer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

using cc::NinePatchLayer;

namespace WebKit {

WebNinePatchLayerImpl::WebNinePatchLayerImpl()
    : m_layer(new WebLayerImpl(NinePatchLayer::create()))
{
    m_layer->layer()->setIsDrawable(true);
}

WebNinePatchLayerImpl::~WebNinePatchLayerImpl()
{
}

WebLayer* WebNinePatchLayerImpl::layer()
{
    return m_layer.get();
}

void WebNinePatchLayerImpl::setBitmap(const SkBitmap& bitmap, const WebRect& aperture) {
    static_cast<NinePatchLayer*>(m_layer->layer())->setBitmap(bitmap, gfx::Rect(aperture));
}

} // namespace WebKit
