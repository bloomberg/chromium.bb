// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/VideoPainter.h"

#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "core/loader/EmptyClients.h"
#include "core/paint/StubChromeClientForSPv2.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebSize.h"
#include "testing/gtest/include/gtest/gtest.h"

// Integration tests of video painting code (in SPv2 mode).

namespace blink {
namespace {

class StubWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  StubWebMediaPlayer(WebMediaPlayerClient* client) : client_(client) {}

  const WebLayer* GetWebLayer() { return web_layer_.get(); }

  // WebMediaPlayer
  void Load(LoadType, const WebMediaPlayerSource&, CORSMode) {
    network_state_ = kNetworkStateLoaded;
    client_->NetworkStateChanged();
    ready_state_ = kReadyStateHaveEnoughData;
    client_->ReadyStateChanged();
    web_layer_ = Platform::Current()->CompositorSupport()->CreateLayer();
    client_->SetWebLayer(web_layer_.get());
  }
  NetworkState GetNetworkState() const override { return network_state_; }
  ReadyState GetReadyState() const override { return ready_state_; }

 private:
  WebMediaPlayerClient* client_;
  std::unique_ptr<WebLayer> web_layer_;
  NetworkState network_state_ = kNetworkStateEmpty;
  ReadyState ready_state_ = kReadyStateHaveNothing;
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  // LocalFrameClient
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client) override {
    return WTF::MakeUnique<StubWebMediaPlayer>(client);
  }
};

class VideoPainterTestForSPv2 : public ::testing::Test,
                                private ScopedSlimmingPaintV2ForTest {
 public:
  VideoPainterTestForSPv2() : ScopedSlimmingPaintV2ForTest(true) {}

 protected:
  void SetUp() override {
    chrome_client_ = new StubChromeClientForSPv2();
    local_frame_client_ = new StubLocalFrameClient;
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    page_holder_ = DummyPageHolder::Create(
        IntSize(800, 600), &clients, local_frame_client_.Get(),
        [](Settings& settings) {
          settings.SetAcceleratedCompositingEnabled(true);
        });
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    GetDocument().SetURL(KURL(NullURL(), "https://example.com/"));
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }
  bool HasLayerAttached(const WebLayer& layer) {
    return chrome_client_->HasLayer(layer);
  }

 private:
  Persistent<StubChromeClientForSPv2> chrome_client_;
  Persistent<StubLocalFrameClient> local_frame_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(VideoPainterTestForSPv2, VideoLayerAppearsInLayerTree) {
  // Insert a <video> and allow it to begin loading.
  GetDocument().body()->setInnerHTML(
      "<video width=300 height=200 src=test.ogv>");
  testing::RunPendingTasks();

  // Force the page to paint.
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Fetch the layer associated with the <video>, and check that it was
  // correctly configured in the layer tree.
  HTMLMediaElement* element =
      ToHTMLMediaElement(GetDocument().body()->firstChild());
  StubWebMediaPlayer* player =
      static_cast<StubWebMediaPlayer*>(element->GetWebMediaPlayer());
  const WebLayer* layer = player->GetWebLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(HasLayerAttached(*layer));
  EXPECT_EQ(WebSize(300, 200), layer->Bounds());
}

}  // namespace
}  // namespace blink
