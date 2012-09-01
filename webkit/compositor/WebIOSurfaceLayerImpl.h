// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIOSurfaceLayerImpl_h
#define WebIOSurfaceLayerImpl_h

#include <public/WebIOSurfaceLayer.h>
#include <wtf/OwnPtr.h>

namespace WebKit {

class WebIOSurfaceLayerImpl : public WebIOSurfaceLayer {
public:
    WebIOSurfaceLayerImpl();
    virtual ~WebIOSurfaceLayerImpl();

    // WebIOSurfaceLayer implementation.
    virtual WebLayer* layer() OVERRIDE;
    virtual void setIOSurfaceProperties(unsigned ioSurfaceId, WebSize) OVERRIDE;

private:
    OwnPtr<WebLayerImpl> m_layer;
};

}

#endif // WebIOSurfaceLayerImpl_h

