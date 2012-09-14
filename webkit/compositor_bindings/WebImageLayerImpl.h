// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebImageLayerImpl_h
#define WebImageLayerImpl_h

#include <public/WebImageLayer.h>
#include <wtf/OwnPtr.h>

namespace WebKit {
class WebLayerImpl;

class WebImageLayerImpl : public WebImageLayer {
public:
    WebImageLayerImpl();
    virtual ~WebImageLayerImpl();

    // WebImageLayer implementation.
    WebLayer* layer() OVERRIDE;
    virtual void setBitmap(SkBitmap) OVERRIDE;

private:
    OwnPtr<WebLayerImpl> m_layer;
};

}

#endif // WebImageLayerImpl_h
