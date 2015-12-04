// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeGraphicsLayerFactory_h
#define FakeGraphicsLayerFactory_h

#include "platform/graphics/GraphicsLayerFactory.h"

namespace blink {

class FakeGraphicsLayerFactory : public GraphicsLayerFactory {
public:
    static FakeGraphicsLayerFactory* instance();

    // GraphicsLayerFactory
    PassOwnPtr<GraphicsLayer> createGraphicsLayer(GraphicsLayerClient*) override;

private:
    FakeGraphicsLayerFactory();
};

} // namespace blink

#endif // FakeGraphicsLayerFactory_h
