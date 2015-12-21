// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/layer.h"
#include "cc/layers/layer_settings.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

PaintArtifactCompositor::PaintArtifactCompositor()
{
}

PaintArtifactCompositor::~PaintArtifactCompositor()
{
}

void PaintArtifactCompositor::initializeIfNeeded()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    if (m_rootLayer)
        return;

    m_rootLayer = cc::Layer::Create(cc::LayerSettings());
    m_webLayer = adoptPtr(Platform::current()->compositorSupport()->createLayerFromCCLayer(m_rootLayer.get()));
}

void PaintArtifactCompositor::update(const PaintArtifact& paintArtifact)
{
    initializeIfNeeded();
    ASSERT(m_rootLayer);
}

} // namespace blink
