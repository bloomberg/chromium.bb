// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/public/common/mobile_metrics/mobile_friendliness.h"
#include "third_party/blink/public/mojom/mobile_metrics/mobile_friendliness.mojom-shared.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_metrics_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

using mobile_metrics_test_helpers::MobileFriendlinessTree;
using mojom::ViewportStatus;

static constexpr char kBaseUrl[] = "http://www.test.com/";
class MobileFriendlinessCheckerTest : public testing::Test {
 public:
  ~MobileFriendlinessCheckerTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  static void ConfigureAndroidSettings(WebSettings* settings) {
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
  }

  MobileFriendlinessTree CalculateMetricsForHTMLString(
      const std::string& html,
      float device_scale = 1.0) {
    frame_test_helpers::WebViewHelper helper;
    helper.Initialize(nullptr, nullptr, ConfigureAndroidSettings);
    helper.GetWebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(
        device_scale);
    helper.Resize(gfx::Size(480, 800));
    frame_test_helpers::LoadHTMLString(helper.GetWebView()->MainFrameImpl(),
                                       html,
                                       url_test_helpers::ToKURL("about:blank"));
    return MobileFriendlinessTree::GetMobileFriendlinessTree(
        helper.GetWebView()->MainFrameImpl()->GetFrameView());
  }

  MobileFriendlinessTree CalculateMetricsForFile(const std::string& path,
                                                 float device_scale = 1.0) {
    frame_test_helpers::WebViewHelper helper;
    helper.Initialize(nullptr, nullptr, ConfigureAndroidSettings);
    helper.GetWebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(
        device_scale);
    helper.Resize(gfx::Size(480, 800));
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
        WebString::FromUTF8(path));
    frame_test_helpers::LoadFrame(helper.GetWebView()->MainFrameImpl(),
                                  kBaseUrl + path);
    return MobileFriendlinessTree::GetMobileFriendlinessTree(
        helper.GetWebView()->MainFrameImpl()->GetFrameView());
  }

  MobileFriendliness CalculateMainFrameMetricsForHTMLString(
      const std::string& html,
      float device_scale = 1.0) {
    return CalculateMetricsForHTMLString(html, device_scale).mf;
  }

  MobileFriendliness CalculateMainFrameMetricsForFile(
      const std::string& path,
      float device_scale = 1.0) {
    return CalculateMetricsForFile(path, device_scale).mf;
  }

  void SetUseZoomForDSF(bool use_zoom_for_dsf) {
    platform_->SetUseZoomForDSF(use_zoom_for_dsf);
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(MobileFriendlinessCheckerTest, NoViewportSetting) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString("<body>bar</body>");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidth) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-1.html");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewport) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-30.html");
  EXPECT_EQ(actual_mf.viewport_device_width, blink::mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 200);
}

TEST_F(MobileFriendlinessCheckerTest, HardcodedViewportWithDeviceScale3) {
  SetUseZoomForDSF(true);
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-30.html",
                                       /*device_scale=*/3.0);
  EXPECT_EQ(actual_mf.viewport_device_width, blink::mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 200);
}

TEST_F(MobileFriendlinessCheckerTest, DeviceWidthWithInitialScale05) {
  // Specifying initial-scale=0.5 is usually not the best choice for most web
  // pages. But we cannot determine that such page must not be mobile friendly.
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForFile("viewport/viewport-34.html");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 5);
}

TEST_F(MobileFriendlinessCheckerTest, UserZoom) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForFile(
      "viewport-initial-scale-and-user-scalable-no.html");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 20);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, NoText) {
  MobileFriendliness actual_mf =
      CalculateMainFrameMetricsForHTMLString(R"(<body></body>)");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NoSmallFonts) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<div style="font-size: 12px">
  This is legible font size example.
</div>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, OnlySmallFonts) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<div style="font-size:7px">
  Small font text.
</div>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<div style="font-size:12px">
  legible text.
  <div style="font-size:8px">
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
    The quick brown fox jumps over the lazy dog.<br>
  </div>
</div>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, MostlySmallInSpan) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<div style="font-size: 12px">
  x
  <span style="font-size:8px">
    This is the majority part of the document.
  </span>
  y
</div>
)");
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 80);
}

TEST_F(MobileFriendlinessCheckerTest, MultipleDivs) {
  MobileFriendliness actual_mf = CalculateMetricsForHTMLString(R"(
<div style="font-size: 12px">
  x
  <div style="font-size:8px">
    middle of div
    <div style="font-size:1px">
      inner of div
    </div>
  </div>
  y
</div>
)")
                                     .mf;
  EXPECT_LT(actual_mf.small_text_ratio, 100);
  EXPECT_GT(actual_mf.small_text_ratio, 68);
}

TEST_F(MobileFriendlinessCheckerTest, DontCountInvisibleSmallFontArea) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <div style="font-size: 12px">
      x
      <div style="font-size:4px;display:none;">
        this is an invisible string.
      </div>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleZoomedLegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=10">
  </head>
  <body style="font-size: 5px">
    Legible text in 50px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 100);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ViewportZoomedOutIllegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, initial-scale=0.5">
  </head>
  <body style="font-size: 16px; width: 960px">
    Illegible text in 8px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 480);
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 5);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, TooWideViewportWidthIllegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=960">
  </head>
  <body style="font-size: 12px">
    Illegible text in 6px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, blink::mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.viewport_hardcoded_width, 960);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, CSSZoomedIllegibleFont) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body style="font-size: 12px; zoom:50%">
    Illegible text in 6px.
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(actual_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(actual_mf.small_text_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, TextNarrow) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <pre>foo foo foo foo foo</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWide) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <pre>)" +
      std::string(10000, 'a') +
      R"(</pre>
  </body>
</html>
)");
  EXPECT_NE(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideOpacityZero) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <pre style="opacity:0">)" +
      std::string(10000, 'a') +
      R"(</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideVisibilityHidden) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <pre style="visibility:hidden">)" +
      std::string(10000, 'a') + R"(</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHidden) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <pre style="overflow:hidden">)" +
      std::string(10000, 'a') +
      R"(</pre>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHiddenInDiv) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <div style="overflow:hidden">
      <pre>)" +
      std::string(10000, 'a') +
      R"(
      </pre>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TextTooWideHiddenInDivDiv) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(
      R"(
<html>
  <body>
    <div style="overflow:hidden">
      <div>
        <pre>)" +
      std::string(10000, 'a') +
      R"(
        </pre>
      <div>
    </div>
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageNarrow) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:200px; height:50px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWide) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:2000px; height:50px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 317);
}

TEST_F(MobileFriendlinessCheckerTest, ImageAbsolutePosition) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:100px; height:100px; position:absolute; left:2000px">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 338);
}

TEST_F(MobileFriendlinessCheckerTest, ImageTooWideDisplayNone) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <body>
    <img style="width:2000px; height:50px; display:none">
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.text_content_outside_viewport_percentage, 0);
}

TEST_F(MobileFriendlinessCheckerTest, ScaleTextOutsideViewport) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
<html>
  <head>
    <meta name="viewport" content="width=480, minimum-scale=1, initial-scale=3">
  </head>
  <body style="font-size: 76px; width: 480">
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
    foo foo foo foo foo foo foo foo foo foo
  </body>
</html>
)");
  EXPECT_EQ(actual_mf.viewport_initial_scale_x10, 30);
  EXPECT_GE(actual_mf.text_content_outside_viewport_percentage, 100.0);
}

TEST_F(MobileFriendlinessCheckerTest, SingleTapTarget) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a onclick="alert('clicked');">
      link
    </a>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, NoBadTapTarget) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="width:30px; height:30px">
      a
    </button>
    <button style="width:30px; height:30px">
      b
    </button>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, TooCloseTapTargetsVertical) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 400px;height: 400px; margin: 0px">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px">
        B
      </div>
    </a>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 50);
}

TEST_F(MobileFriendlinessCheckerTest, TooCloseTapTargetsVerticalSamePoint) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 400px;height: 400px; margin: 0px">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px">
        B
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 400px;height: 400px; margin: 0px">
        C
      </div>
    </a>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 33);
}

TEST_F(MobileFriendlinessCheckerTest, TooCloseTapTargetsHorizontal) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 300px;height: 300px; margin: 0px; display:inline-block">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px; display:inline-block">
        B
      </div>
    </a>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 50);
}

TEST_F(MobileFriendlinessCheckerTest, TooCloseTapTargetsHorizontalSamePoint) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <a href="about:blank">
      <div style="width: 200px;height: 200px; margin: 0px; display:inline-block">
        A
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 10px;height: 10px; margin: 0px; display:inline-block">
        B
      </div>
    </a>
    <a href="about:blank">
      <div style="width: 200px;height: 200px; margin: 0px; display:inline-block">
        C
      </div>
    </a>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 33);
}

TEST_F(MobileFriendlinessCheckerTest, GridGoodTargets3X3) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <div>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          1-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          2-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          3-1
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          1-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          2-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          3-2
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          1-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          2-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 50px;height: 50px; margin: 50px; display:inline-block">
          3-3
        </div>
      </a>
    </div>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, GridBadTargets3X3) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <div>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          1-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          2-1
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          3-1
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          1-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          2-2
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          3-2
        </div>
      </a>
    </div>
    <div>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          1-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          2-3
        </div>
      </a>
      <a href="about:blank">
        <div style="width: 10px;height: 10px; margin: 10px; display:inline-block">
          3-3
        </div>
      </a>
    </div>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, FormTapTargets) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <form>
      <input style="height: 400px; margin: 0px"><br>
      <input style="height: 10px; margin: 0px">
    </form>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 50);
}

TEST_F(MobileFriendlinessCheckerTest, InvisibleTapTargetWillBeIgnored) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
vv  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <form>
      <input style="height: 400px; margin: 0px"><br>
      <div style="display:none">
        <input style="height: 10px; margin: 0px">
      </div>
    </form>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 0);
}

TEST_F(MobileFriendlinessCheckerTest, BadTapTargetWithPositionAbsolute) {
  MobileFriendliness actual_mf = CalculateMainFrameMetricsForHTMLString(R"(
  <head>
    <meta name="viewport" content="width=480, initial-scale=1">
  </head>
  <body style="font-size: 18px">
    <button style="position:absolute; width:50px; height:50px">
      a
    </button>
    <button style="position:relative; width:50px; height:50px">
      b
    </button>
  </body>
)");
  EXPECT_EQ(actual_mf.bad_tap_targets_ratio, 100);
}

TEST_F(MobileFriendlinessCheckerTest, IFrameTest) {
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(kBaseUrl), blink::test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));
  MobileFriendlinessTree actual_mf_tree =
      CalculateMetricsForFile("single_iframe.html");
  const MobileFriendliness& mainframe_mf = actual_mf_tree.mf;
  EXPECT_EQ(mainframe_mf.viewport_device_width, mojom::ViewportStatus::kNo);
  EXPECT_EQ(mainframe_mf.allow_user_zoom, mojom::ViewportStatus::kYes);
  EXPECT_EQ(mainframe_mf.bad_tap_targets_ratio, 0);

  EXPECT_EQ(actual_mf_tree.children.size(), 1u);
  const MobileFriendliness& subframe_mf = actual_mf_tree.children[0].mf;
  EXPECT_EQ(subframe_mf.bad_tap_targets_ratio, 0);
}

}  // namespace blink
