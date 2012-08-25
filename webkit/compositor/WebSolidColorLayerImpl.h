// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSolidColorLayerImpl_h
#define WebSolidColorLayerImpl_h

#include <public/WebSolidColorLayer.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
class SolidColorLayerChromium;
}

namespace WebKit {
class WebLayerImpl;

class WebSolidColorLayerImpl : public WebSolidColorLayer {
public:
    explicit WebSolidColorLayerImpl(PassRefPtr<WebCore::SolidColorLayerChromium>);
    virtual ~WebSolidColorLayerImpl();

    // WebSolidColorLayer implementation.
    virtual WebLayer* layer() OVERRIDE;
    virtual void setBackgroundColor(WebColor) OVERRIDE;

private:
    OwnPtr<WebLayerImpl> m_layer;
};

} // namespace WebKit

#endif // WebSolidColorLayerImpl_h

