// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDelegatedRendererLayerImpl_h
#define WebDelegatedRendererLayerImpl_h

#include "WebLayerImpl.h"
#include <public/WebDelegatedRendererLayer.h>
#include <wtf/OwnPtr.h>

namespace WebKit {

class WebDelegatedRendererLayerImpl : public WebDelegatedRendererLayer {
public:
    WebDelegatedRendererLayerImpl();

    // WebDelegatedRendererLayer implementation.
    virtual WebLayer* layer() OVERRIDE;

protected:
    virtual ~WebDelegatedRendererLayerImpl();

private:
    OwnPtr<WebLayerImpl> m_layer;
};

} // namespace WebKit

#endif // WebDelegatedLayerImpl_h
