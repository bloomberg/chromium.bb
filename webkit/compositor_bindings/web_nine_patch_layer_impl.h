// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNinePatchLayerImpl_h
#define WebNinePatchLayerImpl_h

#include "web_layer_impl.h"
#include "base/memory/scoped_ptr.h"
#include "SkBitmap.h"

namespace WebKit {

class WebLayerImpl;

class WebNinePatchLayerImpl {
public:
    WebNinePatchLayerImpl();
    virtual ~WebNinePatchLayerImpl();

    WebLayer* layer();

    void setBitmap(const SkBitmap& bitmap, const WebRect& aperture);

private:
    scoped_ptr<WebLayerImpl> m_layer;
};

} // namespace WebKit

#endif
