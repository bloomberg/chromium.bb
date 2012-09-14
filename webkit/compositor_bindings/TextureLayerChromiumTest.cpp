// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "TextureLayerChromium.h"

#include "CCLayerTreeHost.h"
#include "FakeCCLayerTreeHostClient.h"
#include "WebCompositorInitializer.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace cc;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace {

class MockCCLayerTreeHost : public CCLayerTreeHost {
public:
    MockCCLayerTreeHost()
        : CCLayerTreeHost(&m_fakeClient, CCLayerTreeSettings())
    {
        initialize();
    }

    MOCK_METHOD0(acquireLayerTextures, void());

private:
    FakeCCLayerTreeHostClient m_fakeClient;
};


class TextureLayerChromiumTest : public testing::Test {
public:
    TextureLayerChromiumTest()
        : m_compositorInitializer(0)
    {
    }

protected:
    virtual void SetUp()
    {
        m_layerTreeHost = adoptPtr(new MockCCLayerTreeHost);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();
    }

    OwnPtr<MockCCLayerTreeHost> m_layerTreeHost;
private:
    WebKitTests::WebCompositorInitializer m_compositorInitializer;
};

TEST_F(TextureLayerChromiumTest, syncImplWhenChangingTextureId)
{
    RefPtr<TextureLayerChromium> testLayer = TextureLayerChromium::create(0);
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    EXPECT_EQ(testLayer->layerTreeHost(), m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->setTextureId(2);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->setTextureId(0);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

TEST_F(TextureLayerChromiumTest, syncImplWhenRemovingFromTree)
{
    RefPtr<LayerChromium> rootLayer = LayerChromium::create();
    ASSERT_TRUE(rootLayer);
    RefPtr<LayerChromium> childLayer = LayerChromium::create();
    ASSERT_TRUE(childLayer);
    rootLayer->addChild(childLayer);
    RefPtr<TextureLayerChromium> testLayer = TextureLayerChromium::create(0);
    ASSERT_TRUE(testLayer);
    testLayer->setTextureId(0);
    childLayer->addChild(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(rootLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    childLayer->addChild(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

} // anonymous namespace
