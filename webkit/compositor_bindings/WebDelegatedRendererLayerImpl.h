// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDelegatedRendererLayerImpl_h
#define WebDelegatedRendererLayerImpl_h

#include "WebLayerImpl.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebDelegatedRendererLayer.h"

namespace WebKit {

class WebDelegatedRendererLayerImpl : public WebDelegatedRendererLayer {
public:
    WebDelegatedRendererLayerImpl();

    // WebDelegatedRendererLayer implementation.
    virtual WebLayer* layer() OVERRIDE;

protected:
    virtual ~WebDelegatedRendererLayerImpl();

private:
    scoped_ptr<WebLayerImpl> m_layer;
};

} // namespace WebKit

#endif // WebDelegatedLayerImpl_h
