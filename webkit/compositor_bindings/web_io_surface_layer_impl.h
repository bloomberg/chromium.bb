// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIOSurfaceLayerImpl_h
#define WebIOSurfaceLayerImpl_h

#include "third_party/WebKit/Source/Platform/chromium/public/WebIOSurfaceLayer.h"
#include "base/memory/scoped_ptr.h"

namespace WebKit {

class WebIOSurfaceLayerImpl : public WebIOSurfaceLayer {
public:
    WebIOSurfaceLayerImpl();
    virtual ~WebIOSurfaceLayerImpl();

    // WebIOSurfaceLayer implementation.
    virtual WebLayer* layer() OVERRIDE;
    virtual void setIOSurfaceProperties(unsigned ioSurfaceId, WebSize) OVERRIDE;

private:
    scoped_ptr<WebLayerImpl> m_layer;
};

}

#endif // WebIOSurfaceLayerImpl_h

