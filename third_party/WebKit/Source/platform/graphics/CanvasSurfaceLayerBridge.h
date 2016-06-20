// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasSurfaceLayerBridge_h
#define CanvasSurfaceLayerBridge_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "wtf/OwnPtr.h"

namespace cc {
// TODO(611796): replace SolidColorLayer with SurfaceLayer
class SolidColorLayer;
}

namespace blink {

class WebLayer;

class PLATFORM_EXPORT CanvasSurfaceLayerBridge {
public:
    explicit CanvasSurfaceLayerBridge();
    ~CanvasSurfaceLayerBridge();
    WebLayer* getWebLayer() const { return m_webLayer.get(); }

private:
    scoped_refptr<cc::SolidColorLayer> m_solidColorLayer;
    OwnPtr<WebLayer> m_webLayer;
};

}

#endif // CanvasSurfaceLayerBridge
