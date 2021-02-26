// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

// Mock content::ColorChooser to test whether End() is called.
class MockColorChooser : public content::ColorChooser {
 public:
  MockColorChooser() = default;
  ~MockColorChooser() override = default;

  MOCK_METHOD0(End, void());
  MOCK_METHOD1(SetSelectedColor, void(SkColor color));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockColorChooser);
};

// Delegate to override OpenColorChooser.
class OpenColorChooserDelegate : public WebContentsDelegate {
 public:
  explicit OpenColorChooserDelegate(
      std::unique_ptr<MockColorChooser> mock_color_chooser)
      : mock_color_chooser_(std::move(mock_color_chooser)) {}

  ~OpenColorChooserDelegate() override = default;

  // WebContentsDelegate:
  ColorChooser* OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override {
    return std::move(mock_color_chooser_).release();
  }

 private:
  std::unique_ptr<MockColorChooser> mock_color_chooser_;

  DISALLOW_COPY_AND_ASSIGN(OpenColorChooserDelegate);
};

}  // namespace

class ColorChooserUnitTest : public RenderViewHostImplTestHarness {};

TEST_F(ColorChooserUnitTest, ColorChooserCallsEndOnNavigatingAway) {
  GURL kUrl1("https://foo.com");
  GURL kUrl2("https://bar.com");

  // Navigate to A.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl1);

  // End should be called at least once on navigating to a new URL.
  std::unique_ptr<MockColorChooser> mock_color_chooser =
      std::make_unique<MockColorChooser>();
  EXPECT_CALL(*mock_color_chooser.get(), End()).Times(testing::AtLeast(1));

  // Set OpenColorChooserDelegate as the new WebContentsDelegate.
  std::unique_ptr<OpenColorChooserDelegate> delegate =
      std::make_unique<OpenColorChooserDelegate>(std::move(mock_color_chooser));
  contents()->SetDelegate(delegate.get());

  mojo::PendingRemote<blink::mojom::ColorChooserClient> pending_client;
  mojo::Remote<blink::mojom::ColorChooser> pending_remote;
  mojo::PendingReceiver<blink::mojom::ColorChooser> pending_receiver =
      pending_remote.BindNewPipeAndPassReceiver();

  // Call WebContentsImpl::OpenColorChooser.
  static_cast<WebContentsImpl*>(contents())
      ->OpenColorChooser(std::move(pending_receiver), std::move(pending_client),
                         SkColorSetRGB(0, 0, 1), {});

  // Navigate to B.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl2);

  contents()->SetDelegate(nullptr);
}

// Run tests with BackForwardCache.
class ColorChooserTestWithBackForwardCache : public ColorChooserUnitTest {
 public:
  ColorChooserTestWithBackForwardCache() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache, {GetFeatureParams()}}},
        /*disabled_features=*/{});
  }

 protected:
  base::FieldTrialParams GetFeatureParams() {
    return {{"TimeToLiveInBackForwardCacheInSeconds", "3600"},
            {"service_worker_supported", "true"}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ColorChooserTestWithBackForwardCache,
       ColorChooserCallsEndOnEnteringBackForwardCache) {
  ASSERT_TRUE(IsBackForwardCacheEnabled());
  GURL kUrl1("https://foo.com");
  GURL kUrl2("https://bar.com");

  // Navigate to A.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl1);
  RenderFrameHost* rfh_a = contents()->GetMainFrame();

  // End should be called at least once on navigating to a new URL.
  std::unique_ptr<MockColorChooser> mock_color_chooser =
      std::make_unique<MockColorChooser>();
  EXPECT_CALL(*mock_color_chooser.get(), End()).Times(testing::AtLeast(1));

  // Set OpenColorChooserDelegate as the new WebContentsDelegate.
  std::unique_ptr<OpenColorChooserDelegate> delegate =
      std::make_unique<OpenColorChooserDelegate>(std::move(mock_color_chooser));
  contents()->SetDelegate(delegate.get());

  mojo::PendingRemote<blink::mojom::ColorChooserClient> pending_client;
  mojo::Remote<blink::mojom::ColorChooser> pending_remote;
  mojo::PendingReceiver<blink::mojom::ColorChooser> pending_receiver =
      pending_remote.BindNewPipeAndPassReceiver();

  // Call WebContentsImpl::OpenColorChooser.
  static_cast<WebContentsImpl*>(contents())
      ->OpenColorChooser(std::move(pending_receiver), std::move(pending_client),
                         SkColorSetRGB(0, 0, 1), {});

  // Navigate to B, A enters BackForwardCache.
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl2);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  contents()->SetDelegate(nullptr);
}

}  // namespace content
