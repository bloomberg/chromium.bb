// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <tuple>

#include "base/optional.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

Vector<char> ReadTestImage() {
  return test::ReadFromFile(test::CoreTestDataPath("notifications/500x500.png"))
      ->CopyAs<Vector<char>>();
}

class LazyLoadCSSImagesTest : public SimTest {
 protected:
  LazyLoadCSSImagesTest()
      : scoped_lazy_image_loading_for_test_(true),
        scoped_automatic_lazy_image_loading_for_test_(true) {}
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
            background-image: url('img.png');
          }
          </style>
          <div style='height:10000px;'></div>
          <div id="deferred_image"></div>
        )HTML"));
    GetDocument().UpdateStyleAndLayoutTree();
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
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
};

TEST_F(LazyLoadCSSImagesTest, CSSBackgroundImageLoadedWithoutLazyLoad) {
  SetLazyLoadEnabled(false);
  SimRequest image_resource("https://example.com/img.png", "image/png");
  LoadMainResource();
  image_resource.Complete(ReadTestImage());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ExpectCSSBackgroundImageDeferredState(false);
}

TEST_F(LazyLoadCSSImagesTest, CSSBackgroundImageDeferredWithLazyLoad) {
  SetLazyLoadEnabled(true);
  LoadMainResource();
  Compositor().BeginFrame();
  test::RunPendingTasks();

  ExpectCSSBackgroundImageDeferredState(true);

  // Scroll down until the background image is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 10000), kProgrammaticScroll);
  SimRequest image_resource("https://example.com/img.png", "image/png");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  image_resource.Complete(ReadTestImage());
  ExpectCSSBackgroundImageDeferredState(false);
}

SimRequestBase::Params BuildRequestParamsForRangeResponse() {
  SimRequestBase::Params params;
  params.response_http_headers = {{"content-range", "bytes 0-2047/9311"}};
  params.response_http_status = 206;
  return params;
}

void ExpectResourceIsFullImage(Resource* resource) {
  EXPECT_TRUE(resource);
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_FALSE(
      resource->GetResourceRequest().HttpHeaderFields().Contains("range"));
  EXPECT_EQ(ResourceType::kImage, resource->GetType());
  EXPECT_FALSE(ToImageResource(resource)->ShouldShowLazyImagePlaceholder());
}

void ExpectResourceIsLazyImagePlaceholder(Resource* resource) {
  EXPECT_TRUE(resource);
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_EQ("bytes=0-2047",
            resource->GetResourceRequest().HttpHeaderField("range"));
  EXPECT_EQ(ResourceType::kImage, resource->GetType());
  EXPECT_TRUE(ToImageResource(resource)->ShouldShowLazyImagePlaceholder());
}

class ScopedDataSaverSetting {
 public:
  explicit ScopedDataSaverSetting(bool is_data_saver_enabled)
      : was_data_saver_previously_enabled_(
            GetNetworkStateNotifier().SaveDataEnabled()) {
    GetNetworkStateNotifier().SetSaveDataEnabled(is_data_saver_enabled);
  }

  ~ScopedDataSaverSetting() {
    GetNetworkStateNotifier().SetSaveDataEnabled(
        was_data_saver_previously_enabled_);
  }

 private:
  const bool was_data_saver_previously_enabled_;
};

enum class LazyImageLoadingFeatureStatus {
  // LazyImageLoading is disabled.
  kDisabled = 0,
  // LazyImageLoading is enabled, but AutomaticLazyImageLoading is disabled.
  kEnabledExplicit,
  // Both LazyImageLoading and AutomaticLazyImageLoading are enabled.
  kEnabledAutomatic,
  // LazyImageLoading, AutomaticLazyImageLoading, and
  // RestrictAutomaticLazyImageLoadingToDataSaver are enabled, while data saver
  // is off.
  kEnabledAutomaticRestrictedAndDataSaverOff,
  // LazyImageLoading, AutomaticLazyImageLoading, and
  // RestrictAutomaticLazyImageLoadingToDataSaver are enabled, while data saver
  // is on.
  kEnabledAutomaticRestrictedAndDataSaverOn,
};

class LazyLoadImagesParamsTest : public SimTest,
                                 public ::testing::WithParamInterface<
                                     std::tuple<LazyImageLoadingFeatureStatus,
                                                WebEffectiveConnectionType>> {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

  LazyLoadImagesParamsTest()
      : scoped_lazy_image_loading_for_test_(
            std::get<LazyImageLoadingFeatureStatus>(GetParam()) !=
            LazyImageLoadingFeatureStatus::kDisabled),
        scoped_automatic_lazy_image_loading_for_test_(
            static_cast<int>(
                std::get<LazyImageLoadingFeatureStatus>(GetParam())) >=
            static_cast<int>(LazyImageLoadingFeatureStatus::kEnabledAutomatic)),
        scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_(
            static_cast<int>(
                std::get<LazyImageLoadingFeatureStatus>(GetParam())) >=
            static_cast<int>(LazyImageLoadingFeatureStatus::
                                 kEnabledAutomaticRestrictedAndDataSaverOff)),
        scoped_data_saver_setting_(
            std::get<LazyImageLoadingFeatureStatus>(GetParam()) ==
            LazyImageLoadingFeatureStatus::
                kEnabledAutomaticRestrictedAndDataSaverOn) {}

  void SetUp() override {
    WebFrameClient().SetEffectiveConnectionTypeForTesting(
        std::get<WebEffectiveConnectionType>(GetParam()));

    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        WebSize(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();

    // These should match the values that would be returned by
    // GetLoadingDistanceThreshold().
    settings.SetLazyImageLoadingDistanceThresholdPxUnknown(200);
    settings.SetLazyImageLoadingDistanceThresholdPxOffline(300);
    settings.SetLazyImageLoadingDistanceThresholdPxSlow2G(400);
    settings.SetLazyImageLoadingDistanceThresholdPx2G(500);
    settings.SetLazyImageLoadingDistanceThresholdPx3G(600);
    settings.SetLazyImageLoadingDistanceThresholdPx4G(700);
    settings.SetLazyLoadEnabled(
        RuntimeEnabledFeatures::LazyImageLoadingEnabled());
  }

  int GetLoadingDistanceThreshold() const {
    static constexpr int kDistanceThresholdByEffectiveConnectionType[] = {
        200, 300, 400, 500, 600, 700};
    return kDistanceThresholdByEffectiveConnectionType[static_cast<int>(
        std::get<WebEffectiveConnectionType>(GetParam()))];
  }

  bool IsAutomaticLazyImageLoadingExpected() const {
    switch (std::get<LazyImageLoadingFeatureStatus>(GetParam())) {
      case LazyImageLoadingFeatureStatus::kDisabled:
      case LazyImageLoadingFeatureStatus::kEnabledExplicit:
      case LazyImageLoadingFeatureStatus::
          kEnabledAutomaticRestrictedAndDataSaverOff:
        return false;
      case LazyImageLoadingFeatureStatus::kEnabledAutomatic:
      case LazyImageLoadingFeatureStatus::
          kEnabledAutomaticRestrictedAndDataSaverOn:
        return true;
    }
    NOTREACHED();
    return false;
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_;
  ScopedDataSaverSetting scoped_data_saver_setting_;
};

TEST_P(LazyLoadImagesParamsTest, NearViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  base::Optional<SimSubresourceRequest> lazy_resource(
      base::in_place, "https://example.com/lazy.png", "image/png",
      RuntimeEnabledFeatures::LazyImageLoadingEnabled()
          ? BuildRequestParamsForRangeResponse()
          : SimRequestBase::Params());
  base::Optional<SimSubresourceRequest> auto_resource(
      base::in_place, "https://example.com/auto.png", "image/png",
      IsAutomaticLazyImageLoadingExpected()
          ? BuildRequestParamsForRangeResponse()
          : SimRequestBase::Params());
  base::Optional<SimSubresourceRequest> unset_resource(
      base::in_place, "https://example.com/unset.png", "image/png",
      IsAutomaticLazyImageLoadingExpected()
          ? BuildRequestParamsForRangeResponse()
          : SimRequestBase::Params());

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <head>
          <link rel='stylesheet' href='https://example.com/style.css' />
        </head>
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/eager.png' loading='eager'
             onload='console.log("eager onload");' />
        <img src='https://example.com/lazy.png' loading='lazy'
             onload='console.log("lazy onload");' />
        <img src='https://example.com/auto.png' loading='auto'
             onload='console.log("auto onload");' />
        <img src='https://example.com/unset.png'
             onload='console.log("unset onload");' />
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() - 100));

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    lazy_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/lazy.png")));
    lazy_resource.emplace("https://example.com/lazy.png", "image/png");
  }

  if (IsAutomaticLazyImageLoadingExpected()) {
    auto_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/auto.png")));
    auto_resource.emplace("https://example.com/auto.png", "image/png");

    unset_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/unset.png")));
    unset_resource.emplace("https://example.com/unset.png", "image/png");
  }

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  eager_resource.Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/eager.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  lazy_resource->Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/lazy.png")));

  auto_resource->Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/auto.png")));

  unset_resource->Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/unset.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

TEST_P(LazyLoadImagesParamsTest, FarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");

  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  base::Optional<SimSubresourceRequest> lazy_resource(
      base::in_place, "https://example.com/lazy.png", "image/png",
      RuntimeEnabledFeatures::LazyImageLoadingEnabled()
          ? BuildRequestParamsForRangeResponse()
          : SimRequestBase::Params());
  base::Optional<SimSubresourceRequest> auto_resource(
      base::in_place, "https://example.com/auto.png", "image/png",
      IsAutomaticLazyImageLoadingExpected()
          ? BuildRequestParamsForRangeResponse()
          : SimRequestBase::Params());
  base::Optional<SimSubresourceRequest> unset_resource(
      base::in_place, "https://example.com/unset.png", "image/png",
      IsAutomaticLazyImageLoadingExpected()
          ? BuildRequestParamsForRangeResponse()
          : SimRequestBase::Params());

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <head>
          <link rel='stylesheet' href='https://example.com/style.css' />
        </head>
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/eager.png' loading='eager'
             onload='console.log("eager onload");' />
        <img src='https://example.com/lazy.png' loading='lazy'
             onload='console.log("lazy onload");' />
        <img src='https://example.com/auto.png' loading='auto'
             onload='console.log("auto onload");' />
        <img src='https://example.com/unset.png'
             onload='console.log("unset onload");' />
        </body>)HTML",
      kViewportHeight + GetLoadingDistanceThreshold() + 100));

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  eager_resource.Complete(full_image);
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/eager.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    lazy_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/lazy.png")));
  } else {
    lazy_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/lazy.png")));
  }

  if (IsAutomaticLazyImageLoadingExpected()) {
    auto_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/auto.png")));

    unset_resource->Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/unset.png")));
  } else {
    auto_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/auto.png")));

    unset_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/unset.png")));
  }

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));

  if (RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
    lazy_resource.emplace("https://example.com/lazy.png", "image/png");
    if (IsAutomaticLazyImageLoadingExpected()) {
      auto_resource.emplace("https://example.com/auto.png", "image/png");
      unset_resource.emplace("https://example.com/unset.png", "image/png");
    }

    // Scroll down so that the images are near the viewport.
    GetDocument().View()->LayoutViewport()->SetScrollOffset(
        ScrollOffset(0, 150), kProgrammaticScroll);

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));

    lazy_resource->Complete(full_image);
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/lazy.png")));

    if (IsAutomaticLazyImageLoadingExpected()) {
      EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
      EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

      auto_resource->Complete(full_image);
      ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
          KURL("https://example.com/auto.png")));

      unset_resource->Complete(full_image);
      ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
          KURL("https://example.com/unset.png")));
    }

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

INSTANTIATE_TEST_SUITE_P(
    LazyImageLoading,
    LazyLoadImagesParamsTest,
    ::testing::Combine(
        ::testing::Values(LazyImageLoadingFeatureStatus::kDisabled,
                          LazyImageLoadingFeatureStatus::kEnabledExplicit,
                          LazyImageLoadingFeatureStatus::kEnabledAutomatic,
                          LazyImageLoadingFeatureStatus::
                              kEnabledAutomaticRestrictedAndDataSaverOff,
                          LazyImageLoadingFeatureStatus::
                              kEnabledAutomaticRestrictedAndDataSaverOn),
        ::testing::Values(WebEffectiveConnectionType::kTypeUnknown,
                          WebEffectiveConnectionType::kTypeOffline,
                          WebEffectiveConnectionType::kTypeSlow2G,
                          WebEffectiveConnectionType::kType2G,
                          WebEffectiveConnectionType::kType3G,
                          WebEffectiveConnectionType::kType4G)));

class LazyLoadAutomaticImagesTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;
  static constexpr int kLoadingDistanceThreshold = 300;

  LazyLoadAutomaticImagesTest()
      : scoped_lazy_image_loading_for_test_(true),
        scoped_automatic_lazy_image_loading_for_test_(true),
        scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_(
            false) {}

  void SetUp() override {
    WebFrameClient().SetEffectiveConnectionTypeForTesting(
        WebEffectiveConnectionType::kTypeUnknown);

    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(
        WebSize(kViewportWidth, kViewportHeight));

    Settings& settings = WebView().GetPage()->GetSettings();
    settings.SetLazyImageLoadingDistanceThresholdPxUnknown(
        kLoadingDistanceThreshold);
    settings.SetLazyFrameLoadingDistanceThresholdPxUnknown(
        kLoadingDistanceThreshold);
    settings.SetLazyLoadEnabled(
        RuntimeEnabledFeatures::LazyImageLoadingEnabled());
  }

  void LoadMainResourceWithImageFarFromViewport(const char* image_attributes) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(String::Format(
        R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <img src='https://example.com/image.png' %s
             onload='console.log("image onload");' />
        </body>)HTML",
        kViewportHeight + kLoadingDistanceThreshold + 100, image_attributes));

    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  void TestLoadImageExpectingLazyLoad(const char* image_attributes) {
    SimSubresourceRequest image_range_resource(
        "https://example.com/image.png", "image/png",
        BuildRequestParamsForRangeResponse());

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    Vector<char> partial_image = ReadTestImage();
    EXPECT_LT(2048U, partial_image.size());
    partial_image.resize(2048U);

    image_range_resource.Complete(partial_image);
    ExpectResourceIsLazyImagePlaceholder(
        GetDocument().Fetcher()->CachedResource(
            KURL("https://example.com/image.png")));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));
  }

  void TestLoadImageExpectingFullImageLoad(const char* image_attributes) {
    SimSubresourceRequest full_resource("https://example.com/image.png",
                                        "image/png");

    LoadMainResourceWithImageFarFromViewport(image_attributes);

    EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
    EXPECT_FALSE(ConsoleMessages().Contains("image onload"));

    full_resource.Complete(ReadTestImage());
    ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
        KURL("https://example.com/image.png")));

    Compositor().BeginFrame();
    test::RunPendingTasks();

    EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
    EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
  }

 private:
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test_;
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test_;
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test_;
};

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromLazyToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='lazy'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromAutoToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image' loading='auto'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, AttributeChangedFromUnsetToEager) {
  TestLoadImageExpectingLazyLoad("id='my_image'");

  SimSubresourceRequest full_resource("https://example.com/image.png",
                                      "image/png");
  GetDocument()
      .getElementById("my_image")
      ->setAttribute(html_names::kLoadingAttr, "eager");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  full_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWithLazyAttr) {
  TestLoadImageExpectingLazyLoad("loading='lazy' width='1px' height='1px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWithLazyAttr) {
  TestLoadImageExpectingLazyLoad(
      "loading='lazy' style='width:1px;height:1px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth1Height1) {
  TestLoadImageExpectingFullImageLoad("width='1px' height='1px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth10Height10) {
  TestLoadImageExpectingFullImageLoad("width='10px' height='10px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageWidth1Height11) {
  TestLoadImageExpectingLazyLoad("width='1px' height='11px'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth1Height1) {
  TestLoadImageExpectingFullImageLoad("style='width:1px;height:1px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth10Height10) {
  TestLoadImageExpectingFullImageLoad("style='width:10px;height:10px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, TinyImageViaStyleWidth11Height1) {
  TestLoadImageExpectingLazyLoad("style='width:11px;height:1px;'");
}

TEST_F(LazyLoadAutomaticImagesTest, JavascriptCreatedImageFarFromViewport) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <script>
          var my_image = new Image(50, 50);
          my_image.onload = function() { console.log('my_image onload'); };
          my_image.src = 'https://example.com/image.png';
          document.body.appendChild(my_image);
        </script>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("my_image onload"));

  image_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("my_image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, JavascriptCreatedImageAddedAfterLoad) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <script>
          var my_image = new Image(50, 50);
          my_image.onload = function() {
            console.log('my_image onload');
            document.body.appendChild(this);
          };
          my_image.src = 'https://example.com/image.png';
        </script>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_FALSE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("my_image onload"));

  image_resource.Complete(ReadTestImage());
  ExpectResourceIsFullImage(GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/image.png")));

  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("my_image onload"));
}

TEST_F(LazyLoadAutomaticImagesTest, ImageInsideLazyLoadedFrame) {
  ScopedLazyFrameLoadingForTest scoped_lazy_frame_loading_for_test(true);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(String::Format(
      R"HTML(
        <body onload='console.log("main body onload");'>
        <div style='height: %dpx;'></div>
        <iframe src='https://example.com/child_frame.html' loading='lazy'
                id='child_frame' width='300px' height='300px'
                onload='console.log("child frame onload");'></iframe>
        </body>)HTML",
      kViewportHeight + kLoadingDistanceThreshold + 100));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame onload"));

  SimRequest child_frame_resource("https://example.com/child_frame.html",
                                  "text/html");

  // Scroll down so that the iframe is near the viewport, but the images within
  // it aren't near the viewport yet.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 150),
                                                          kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  SimSubresourceRequest css_resource("https://example.com/style.css",
                                     "text/css");
  SimSubresourceRequest eager_resource("https://example.com/eager.png",
                                       "image/png");
  SimSubresourceRequest lazy_range_resource(
      "https://example.com/lazy.png", "image/png",
      BuildRequestParamsForRangeResponse());
  SimSubresourceRequest auto_resource("https://example.com/auto.png",
                                      "image/png");
  SimSubresourceRequest unset_resource("https://example.com/unset.png",
                                       "image/png");

  child_frame_resource.Complete(R"HTML(
      <head>
        <link rel='stylesheet' href='https://example.com/style.css' />
      </head>
      <body onload='window.parent.console.log("child body onload");'>
      <div style='height: 100px;'></div>
      <img src='https://example.com/eager.png' loading='eager'
           onload='window.parent.console.log("eager onload");' />
      <img src='https://example.com/lazy.png' loading='lazy'
           onload='window.parent.console.log("lazy onload");' />
      <img src='https://example.com/auto.png' loading='auto'
           onload='window.parent.console.log("auto onload");' />
      <img src='https://example.com/unset.png'
           onload='window.parent.console.log("unset onload");' />
      </body>)HTML");

  css_resource.Complete("img { width: 50px; height: 50px; }");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("child body onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("auto onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("unset onload"));

  Vector<char> full_image = ReadTestImage();
  ASSERT_LT(2048U, full_image.size());
  Vector<char> partial_image;
  partial_image.Append(full_image.data(), 2048U);

  Document* child_frame_document =
      ToHTMLIFrameElement(GetDocument().getElementById("child_frame"))
          ->contentDocument();

  eager_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/eager.png")));

  lazy_range_resource.Complete(partial_image);
  ExpectResourceIsLazyImagePlaceholder(
      child_frame_document->Fetcher()->CachedResource(
          KURL("https://example.com/lazy.png")));

  auto_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/auto.png")));

  unset_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/unset.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_FALSE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));

  SimSubresourceRequest lazy_resource("https://example.com/lazy.png",
                                      "image/png");

  // Scroll down so that all the images in the iframe are near the viewport.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 250),
                                                          kProgrammaticScroll);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  lazy_resource.Complete(full_image);
  ExpectResourceIsFullImage(child_frame_document->Fetcher()->CachedResource(
      KURL("https://example.com/lazy.png")));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(ConsoleMessages().Contains("main body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child frame onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("child body onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("eager onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("lazy onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("auto onload"));
  EXPECT_TRUE(ConsoleMessages().Contains("unset onload"));
}

}  // namespace

}  // namespace blink
