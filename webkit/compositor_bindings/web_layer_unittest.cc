// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayerClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerScrollClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSolidColorLayer.h"
#include "webkit/compositor_bindings/test/web_layer_tree_view_test_common.h"
#include "webkit/compositor_bindings/web_content_layer_impl.h"
#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_tree_view_impl_for_testing.h"
#include "webkit/compositor_bindings/web_solid_color_layer_impl.h"

using namespace WebKit;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::Test;
using testing::_;

namespace {

class MockWebContentLayerClient : public WebContentLayerClient {
public:
    MOCK_METHOD4(paintContents, void(WebCanvas*, const WebRect& clip, bool canPaintLCDText, WebFloatRect& opaque));
};

class WebLayerTest : public Test {
public:
    WebLayerTest()
    {
    }

    virtual void SetUp()
    {
        m_rootLayer.reset(new WebLayerImpl);
        EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
        m_view.reset(new WebLayerTreeViewImplForTesting(
            WebLayerTreeViewImplForTesting::FAKE_CONTEXT, &m_client));
        EXPECT_TRUE(m_view->initialize(scoped_ptr<cc::Thread>(NULL)));
        m_view->setRootLayer(*m_rootLayer);
        EXPECT_TRUE(m_view);
        Mock::VerifyAndClearExpectations(&m_client);
    }

    virtual void TearDown()
    {
        // We may get any number of scheduleComposite calls during shutdown.
        EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
        m_rootLayer.reset();
        m_view.reset();
    }

protected:
    MockWebLayerTreeViewClient m_client;
    scoped_ptr<WebLayer> m_rootLayer;
    scoped_ptr<WebLayerTreeViewImplForTesting> m_view;
};

// Tests that the client gets called to ask for a composite if we change the
// fields.
TEST_F(WebLayerTest, Client)
{
    // Base layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    scoped_ptr<WebLayer> layer(new WebLayerImpl);
    layer->setDrawsContent(true);
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
    scoped_ptr<WebLayer> otherLayer(new WebLayerImpl);
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
    scoped_ptr<WebExternalTextureLayer> textureLayer(new WebExternalTextureLayerImpl(NULL));
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
    EXPECT_CALL(contentClient, paintContents(_, _, _, _)).Times(AnyNumber());
                                             
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    scoped_ptr<WebContentLayer> contentLayer(new WebContentLayerImpl(&contentClient));
    m_rootLayer->addChild(contentLayer->layer());
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    contentLayer->layer()->setDrawsContent(false);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_FALSE(contentLayer->layer()->drawsContent());

    // Solid color layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    scoped_ptr<WebSolidColorLayer> solidColorLayer(new WebSolidColorLayerImpl);
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
