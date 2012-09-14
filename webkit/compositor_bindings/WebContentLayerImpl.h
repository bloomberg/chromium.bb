// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebContentLayerImpl_h
#define WebContentLayerImpl_h

#include "ContentLayerChromiumClient.h"
#include "WebLayerImpl.h"
#include <public/WebContentLayer.h>
#include <wtf/OwnPtr.h>

namespace cc {
class IntRect;
class FloatRect;
}

namespace WebKit {
class WebContentLayerClient;

class WebContentLayerImpl : public WebContentLayer,
                            public cc::ContentLayerChromiumClient {
public:
    explicit WebContentLayerImpl(WebContentLayerClient*);

    // WebContentLayer implementation.
    virtual WebLayer* layer() OVERRIDE;
    virtual void setDoubleSided(bool)  OVERRIDE;
    virtual void setBoundsContainPageScale(bool) OVERRIDE;
    virtual bool boundsContainPageScale() const OVERRIDE;
    virtual void setUseLCDText(bool)  OVERRIDE;
    virtual void setDrawCheckerboardForMissingTiles(bool)  OVERRIDE;

protected:
    virtual ~WebContentLayerImpl();

    // ContentLayerChromiumClient implementation.
    virtual void paintContents(SkCanvas*, const cc::IntRect& clip, cc::FloatRect& opaque) OVERRIDE;

    OwnPtr<WebLayerImpl> m_layer;
    WebContentLayerClient* m_client;
    bool m_drawsContent;
};

} // namespace WebKit

#endif // WebContentLayerImpl_h
