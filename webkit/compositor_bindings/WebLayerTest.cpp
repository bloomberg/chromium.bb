// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include <public/WebLayer.h>

#include "CompositorFakeWebGraphicsContext3D.h"
#include "WebCompositorInitializer.h"
#include "WebLayerImpl.h"
#include "WebLayerTreeViewTestCommon.h"
#include <public/WebContentLayer.h>
#include <public/WebContentLayerClient.h>
#include <public/WebExternalTextureLayer.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebLayerScrollClient.h>
#include <public/WebLayerTreeView.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebRect.h>
#include <public/WebSize.h>
#include <public/WebSolidColorLayer.h>

#include <gmock/gmock.h>

using namespace WebKit;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::Test;
using testing::_;

namespace {

class MockWebContentLayerClient : public WebContentLayerClient {
public:
    MOCK_METHOD3(paintContents, void(WebCanvas*, const WebRect& clip, WebFloatRect& opaque));
};

class WebLayerTest : public Test {
public:
    WebLayerTest()
        : m_compositorInitializer(0)
    {
    }

    virtual void SetUp()
    {
        m_rootLayer = adoptPtr(WebLayer::create());
        EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
        EXPECT_TRUE(m_view = adoptPtr(WebLayerTreeView::create(&m_client, *m_rootLayer, WebLayerTreeView::Settings())));
        Mock::VerifyAndClearExpectations(&m_client);
    }

    virtual void TearDown()
    {
        // We may get any number of scheduleComposite calls during shutdown.
        EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
        m_rootLayer.clear();
        m_view.clear();
    }

protected:
    WebKitTests::WebCompositorInitializer m_compositorInitializer;
    MockWebLayerTreeViewClient m_client;
    OwnPtr<WebLayer> m_rootLayer;
    OwnPtr<WebLayerTreeView> m_view;
};

// Tests that the client gets called to ask for a composite if we change the
// fields.
TEST_F(WebLayerTest, Client)
{
    // Base layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    OwnPtr<WebLayer> layer = adoptPtr(WebLayer::create());
    m_rootLayer->addChild(layer.get());
    Mock::VerifyAndClearExpectations(&m_client);

    WebFloatPoint point(3, 4);
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer->setAnchorPoint(point);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(point, layer->anchorPoint());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    float anchorZ = 5;
    layer->setAnchorPointZ(anchorZ);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(anchorZ, layer->anchorPointZ());

    WebSize size(7, 8);
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer->setBounds(size);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(size, layer->bounds());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer->setMasksToBounds(true);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_TRUE(layer->masksToBounds());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    OwnPtr<WebLayer> otherLayer = adoptPtr(WebLayer::create());
    m_rootLayer->addChild(otherLayer.get());
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer->setMaskLayer(otherLayer.get());
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    float opacity = 0.123f;
    layer->setOpacity(opacity);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(opacity, layer->opacity());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer->setOpaque(true);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_TRUE(layer->opaque());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer->setPosition(point);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(point, layer->position());

    // Texture layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    OwnPtr<WebExternalTextureLayer> textureLayer = adoptPtr(WebExternalTextureLayer::create());
    m_rootLayer->addChild(textureLayer->layer());
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    textureLayer->setTextureId(3);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    textureLayer->setFlipped(true);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    WebFloatRect uvRect(0.1f, 0.1f, 0.9f, 0.9f);
    textureLayer->setUVRect(uvRect);
    Mock::VerifyAndClearExpectations(&m_client);


    // Content layer.
    MockWebContentLayerClient contentClient;
    EXPECT_CALL(contentClient, paintContents(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    OwnPtr<WebContentLayer> contentLayer = adoptPtr(WebContentLayer::create(&contentClient));
    m_rootLayer->addChild(contentLayer->layer());
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    contentLayer->layer()->setDrawsContent(false);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_FALSE(contentLayer->layer()->drawsContent());

    // Solid color layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    OwnPtr<WebSolidColorLayer> solidColorLayer = adoptPtr(WebSolidColorLayer::create());
    m_rootLayer->addChild(solidColorLayer->layer());
    Mock::VerifyAndClearExpectations(&m_client);

}

class MockScrollClient : public WebLayerScrollClient {
public:
    MOCK_METHOD0(didScroll, void());
};

TEST_F(WebLayerTest, notifyScrollClient)
{
    MockScrollClient scrollClient;

    EXPECT_CALL(scrollClient, didScroll()).Times(0);
    m_rootLayer->setScrollClient(&scrollClient);
    Mock::VerifyAndClearExpectations(&scrollClient);

    EXPECT_CALL(scrollClient, didScroll()).Times(1);
    m_rootLayer->setScrollPosition(WebPoint(14, 19));
    Mock::VerifyAndClearExpectations(&scrollClient);

    EXPECT_CALL(scrollClient, didScroll()).Times(0);
    m_rootLayer->setScrollPosition(WebPoint(14, 19));
    Mock::VerifyAndClearExpectations(&scrollClient);

    m_rootLayer->setScrollClient(0);
}

}
