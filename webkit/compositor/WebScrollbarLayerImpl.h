// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScrollbarLayerImpl_h
#define WebScrollbarLayerImpl_h

#include <public/WebScrollbarLayer.h>
#include <wtf/OwnPtr.h>

namespace WebKit {
class WebLayerImpl;

class WebScrollbarLayerImpl : public WebScrollbarLayer {
public:
    WebScrollbarLayerImpl(WebScrollbar*, WebScrollbarThemePainter, WebScrollbarThemeGeometry*);
    virtual ~WebScrollbarLayerImpl();

    // WebScrollbarLayer implementation.
    virtual WebLayer* layer() OVERRIDE;
    virtual void setScrollLayer(WebLayer*) OVERRIDE;

private:
    OwnPtr<WebLayerImpl> m_layer;
};

}

#endif // WebScrollbarLayerImpl_h
