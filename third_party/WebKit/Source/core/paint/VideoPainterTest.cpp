// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/video_painter.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_compositor_support.h"
#include "third_party/blink/public/platform/web_layer.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/stub_chrome_client_for_spv2.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

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
  test::RunPendingTasks();

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
