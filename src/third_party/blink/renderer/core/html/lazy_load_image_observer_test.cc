// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class LazyLoadImagesSimTest : public SimTest {
 protected:
  LazyLoadImagesSimTest() : scoped_lazy_image_loading_for_test_(true) {}
  void SetLazyLoadEnabled(bool enabled) {
    WebView().GetPage()->GetSettings().SetLazyLoadEnabled(enabled);
  }

  void LoadMainResource() {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(R"HTML(
          <style>
          #deferred_image {
            height:200px;
            background-image: url('img.jpg');
          }
          </style>
          <div style='height:10000px;'></div>
          <div id="deferred_image"></div>
        )HTML"));
  }

  void ExpectCSSBackgroundImageDeferredState(bool deferred) {
    const ComputedStyle* deferred_image_style =
        GetDocument().getElementById("deferred_image")->GetComputedStyle();
    EXPECT_TRUE(deferred_image_style->HasBackgroundImage());
    bool is_background_image_found = false;
    for (const FillLayer* background_layer =
             &deferred_image_style->BackgroundLayers();
         background_layer; background_layer = background_layer->Next()) {
      if (StyleImage* deferred_image = background_layer->GetImage()) {
        EXPECT_TRUE(deferred_image->IsImageResource());
        EXPECT_EQ(deferred, deferred_image->IsLazyloadPossiblyDeferred());
        EXPECT_NE(deferred, deferred_image->IsLoaded());
        is_background_image_found = true;
      }
    }
    EXPECT_TRUE(is_background_image_found);
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
};

TEST_F(LazyLoadImagesSimTest, CSSBackgroundImageLoadedWithoutLazyLoad) {
  SetLazyLoadEnabled(false);
  SimRequest image_resource("https://example.com/img.jpg", "image/jpeg");
  LoadMainResource();
  image_resource.Complete("");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ExpectCSSBackgroundImageDeferredState(false);
}

TEST_F(LazyLoadImagesSimTest, CSSBackgroundImageDeferredWithLazyLoad) {
  SetLazyLoadEnabled(true);
  LoadMainResource();
  Compositor().BeginFrame();
  test::RunPendingTasks();

  ExpectCSSBackgroundImageDeferredState(true);

  // Scroll down until the background image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 10000), kProgrammaticScroll);
  SimRequest image_resource("https://example.com/img.jpg", "image/jpeg");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  image_resource.Complete("");
  ExpectCSSBackgroundImageDeferredState(false);
}

}  // namespace

}  // namespace blink
