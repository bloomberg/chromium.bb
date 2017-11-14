// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/VideoPainter.h"

#include "core/html/media/HTMLMediaElement.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "core/paint/StubChromeClientForSPv2.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebSize.h"

// Integration tests of video painting code (in SPv2 mode).

namespace blink {
namespace {

class StubWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  StubWebMediaPlayer(WebMediaPlayerClient* client) : client_(client) {}

  const WebLayer* GetWebLayer() { return web_layer_.get(); }

  // WebMediaPlayer
  void Load(LoadType, const WebMediaPlayerSource&, CORSMode) override {
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

class VideoStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  // LocalFrameClient
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient* client,
      WebLayerTreeView* view) override {
    return std::make_unique<StubWebMediaPlayer>(client);
  }
};

class VideoPainterTestForSPv2 : private ScopedSlimmingPaintV2ForTest,
                                public PaintControllerPaintTestBase {
 public:
  VideoPainterTestForSPv2()
      : ScopedSlimmingPaintV2ForTest(true),
        PaintControllerPaintTestBase(new VideoStubLocalFrameClient),
        chrome_client_(new StubChromeClientForSPv2) {}

  void SetUp() override {
    PaintControllerPaintTestBase::SetUp();
    EnableCompositing();
    GetDocument().SetURL(KURL(NullURL(), "https://example.com/"));
  }

  bool HasLayerAttached(const WebLayer& layer) {
    return chrome_client_->HasLayer(layer);
  }

  ChromeClient& GetChromeClient() const override { return *chrome_client_; }

 private:
  Persistent<StubChromeClientForSPv2> chrome_client_;
};

TEST_F(VideoPainterTestForSPv2, VideoLayerAppearsInLayerTree) {
  // Insert a <video> and allow it to begin loading.
  SetBodyInnerHTML("<video width=300 height=200 src=test.ogv>");
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
