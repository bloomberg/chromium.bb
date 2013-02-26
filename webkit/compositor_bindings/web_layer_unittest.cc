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
  MOCK_METHOD4(paintContents,
               void(WebCanvas*,
                    const WebRect& clip,
                    bool can_paint_lcd_text,
                    WebFloatRect& opaque));
};

class WebLayerTest : public Test {
 public:
  WebLayerTest() {}

  virtual void SetUp() {
    root_layer_.reset(new WebLayerImpl);
    EXPECT_CALL(client_, scheduleComposite()).Times(AnyNumber());
    view_.reset(new WebLayerTreeViewImplForTesting(
        WebLayerTreeViewImplForTesting::FAKE_CONTEXT, &client_));
    EXPECT_TRUE(view_->initialize(scoped_ptr<cc::Thread>(NULL)));
    view_->setRootLayer(*root_layer_);
    EXPECT_TRUE(view_);
    Mock::VerifyAndClearExpectations(&client_);
  }

  virtual void TearDown() {
    // We may get any number of scheduleComposite calls during shutdown.
    EXPECT_CALL(client_, scheduleComposite()).Times(AnyNumber());
    root_layer_.reset();
    view_.reset();
  }

 protected:
  MockWebLayerTreeViewClient client_;
  scoped_ptr<WebLayer> root_layer_;
  scoped_ptr<WebLayerTreeViewImplForTesting> view_;
};

// Tests that the client gets called to ask for a composite if we change the
// fields.
TEST_F(WebLayerTest, Client) {
  // Base layer.
  EXPECT_CALL(client_, scheduleComposite()).Times(AnyNumber());
  scoped_ptr<WebLayer> layer(new WebLayerImpl);
  layer->setDrawsContent(true);
  root_layer_->addChild(layer.get());
  Mock::VerifyAndClearExpectations(&client_);

  WebFloatPoint point(3, 4);
  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  layer->setAnchorPoint(point);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_EQ(point, layer->anchorPoint());

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  float anchorZ = 5;
  layer->setAnchorPointZ(anchorZ);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_EQ(anchorZ, layer->anchorPointZ());

  WebSize size(7, 8);
  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  layer->setBounds(size);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_EQ(size, layer->bounds());

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  layer->setMasksToBounds(true);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_TRUE(layer->masksToBounds());

  EXPECT_CALL(client_, scheduleComposite()).Times(AnyNumber());
  scoped_ptr<WebLayer> other_layer_1(new WebLayerImpl);
  root_layer_->addChild(other_layer_1.get());
  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  scoped_ptr<WebLayer> other_layer_2(new WebLayerImpl);
  layer->setMaskLayer(other_layer_2.get());
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  float opacity = 0.123f;
  layer->setOpacity(opacity);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_EQ(opacity, layer->opacity());

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  layer->setOpaque(true);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_TRUE(layer->opaque());

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  layer->setPosition(point);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_EQ(point, layer->position());

  // Texture layer.
  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  scoped_ptr<WebExternalTextureLayer> texture_layer(
      new WebExternalTextureLayerImpl(NULL));
  root_layer_->addChild(texture_layer->layer());
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  texture_layer->setTextureId(3);
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  texture_layer->setFlipped(true);
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  WebFloatRect uv_rect(0.1f, 0.1f, 0.9f, 0.9f);
  texture_layer->setUVRect(uv_rect);
  Mock::VerifyAndClearExpectations(&client_);

  // Content layer.
  MockWebContentLayerClient content_client;
  EXPECT_CALL(content_client, paintContents(_, _, _, _)).Times(AnyNumber());

  EXPECT_CALL(client_, scheduleComposite()).Times(AnyNumber());
  scoped_ptr<WebContentLayer> content_layer(
      new WebContentLayerImpl(&content_client));
  root_layer_->addChild(content_layer->layer());
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  content_layer->layer()->setDrawsContent(false);
  Mock::VerifyAndClearExpectations(&client_);
  EXPECT_FALSE(content_layer->layer()->drawsContent());

  // Solid color layer.
  EXPECT_CALL(client_, scheduleComposite()).Times(AtLeast(1));
  scoped_ptr<WebSolidColorLayer> solid_color_layer(new WebSolidColorLayerImpl);
  root_layer_->addChild(solid_color_layer->layer());
  Mock::VerifyAndClearExpectations(&client_);

}

class MockScrollClient : public WebLayerScrollClient {
 public:
  MOCK_METHOD0(didScroll, void());
};

TEST_F(WebLayerTest, notify_scroll_client) {
  MockScrollClient scroll_client;

  EXPECT_CALL(scroll_client, didScroll()).Times(0);
  root_layer_->setScrollClient(&scroll_client);
  Mock::VerifyAndClearExpectations(&scroll_client);

  EXPECT_CALL(scroll_client, didScroll()).Times(1);
  root_layer_->setScrollPosition(WebPoint(14, 19));
  Mock::VerifyAndClearExpectations(&scroll_client);

  EXPECT_CALL(scroll_client, didScroll()).Times(0);
  root_layer_->setScrollPosition(WebPoint(14, 19));
  Mock::VerifyAndClearExpectations(&scroll_client);

  root_layer_->setScrollClient(0);
}

}
