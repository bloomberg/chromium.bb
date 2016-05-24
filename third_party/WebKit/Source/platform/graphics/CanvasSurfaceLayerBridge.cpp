// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridge.h"

#include "cc/layers/solid_color_layer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"

namespace blink {

CanvasSurfaceLayerBridge::CanvasSurfaceLayerBridge()
{
    m_solidColorLayer = cc::SolidColorLayer::Create();
    m_solidColorLayer->SetBackgroundColor(SK_ColorBLUE);
    m_webLayer = adoptPtr(Platform::current()->compositorSupport()->createLayerFromCCLayer(m_solidColorLayer.get()));
    GraphicsLayer::registerContentsLayer(m_webLayer.get());
}

CanvasSurfaceLayerBridge::~CanvasSurfaceLayerBridge()
{
}

}
