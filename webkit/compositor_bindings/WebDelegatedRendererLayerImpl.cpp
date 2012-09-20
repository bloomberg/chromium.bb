// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebDelegatedRendererLayerImpl.h"

#include "DelegatedRendererLayerChromium.h"

using namespace cc;

namespace WebKit {

WebDelegatedRendererLayerImpl::WebDelegatedRendererLayerImpl()
    : m_layer(adoptPtr(new WebLayerImpl(DelegatedRendererLayerChromium::create())))
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
