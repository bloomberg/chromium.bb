// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutTestHelper.h"

#include "core/loader/EmptyClients.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerFactory.h"

namespace blink {

class FakeGraphicsLayerFactory : public GraphicsLayerFactory {
public:
    PassOwnPtr<GraphicsLayer> createGraphicsLayer(GraphicsLayerClient* client) override
    {
        return adoptPtr(new GraphicsLayer(client));
    }
};

class FakeChromeClient : public EmptyChromeClient {
public:
    static PassOwnPtrWillBeRawPtr<FakeChromeClient> create() { return adoptPtrWillBeNoop(new FakeChromeClient); }

    virtual GraphicsLayerFactory* graphicsLayerFactory() const
    {
        static FakeGraphicsLayerFactory* factory = adoptPtr(new FakeGraphicsLayerFactory).leakPtr();
        return factory;
    }
};

void RenderingTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<FakeChromeClient>, chromeClient, (FakeChromeClient::create()));
    pageClients.chromeClient = chromeClient.get();
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients, nullptr, settingOverrider());

    // This ensures that the minimal DOM tree gets attached
    // correctly for tests that don't call setBodyInnerHTML.
    document().view()->updateAllLifecyclePhases();
}


} // namespace blink
