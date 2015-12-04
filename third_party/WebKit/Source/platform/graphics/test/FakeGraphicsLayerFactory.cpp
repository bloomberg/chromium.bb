// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/test/FakeGraphicsLayerFactory.h"

#include "platform/graphics/GraphicsLayer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"

namespace blink {

FakeGraphicsLayerFactory::FakeGraphicsLayerFactory()
{
}

// static
FakeGraphicsLayerFactory* FakeGraphicsLayerFactory::instance()
{
    DEFINE_STATIC_LOCAL(FakeGraphicsLayerFactory, factory, ());
    return &factory;
}

PassOwnPtr<GraphicsLayer> FakeGraphicsLayerFactory::createGraphicsLayer(GraphicsLayerClient* client)
{
    return adoptPtr(new GraphicsLayer(client));
}

} // namespace blink
