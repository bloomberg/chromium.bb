// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web_delegated_renderer_layer_impl.h"

#include "cc/delegated_renderer_layer.h"

using namespace cc;

namespace WebKit {

WebDelegatedRendererLayerImpl::WebDelegatedRendererLayerImpl()
    : m_layer(new WebLayerImpl(DelegatedRendererLayerChromium::create()))
{
}

WebDelegatedRendererLayerImpl::~WebDelegatedRendererLayerImpl()
{
}

WebLayer* WebDelegatedRendererLayerImpl::layer()
{
    return m_layer.get();
}

} // namespace WebKit
