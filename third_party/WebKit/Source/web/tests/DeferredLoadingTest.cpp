// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/HistogramTester.h"
#include "platform/testing/UnitTestHelpers.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

static const char* kHistogramName =
    "Navigation.DeferredDocumentLoading.StatesV3";

class DeferredLoadingTest : public SimTest {
 protected:
  DeferredLoadingTest() { webView().resize(WebSize(640, 480)); }
  void compositeFrame() {
    compositor().beginFrame();
    testing::runPendingTasks();
    if (compositor().needsBeginFrame())
      compositor().beginFrame();  // VisibleNestedInRight doesn't need this.
    ASSERT_FALSE(compositor().needsBeginFrame());
  }

  std::unique_ptr<SimRequest> createMainResource() {
    std::unique_ptr<SimRequest> mainResource =
        wrapUnique(new SimRequest("https://example.com/", "text/html"));
    loadURL("https://example.com/");
    return mainResource;
  }
};

TEST_F(DeferredLoadingTest, Visible) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete("<iframe sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
}

TEST_F(DeferredLoadingTest, Right) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; left:105vw;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectTotalCount(kHistogramName, 1);
}

TEST_F(DeferredLoadingTest, Below) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; top:105vh;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectTotalCount(kHistogramName, 1);
}

TEST_F(DeferredLoadingTest, Above) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; top:-10000px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadAbove, 1);
}

TEST_F(DeferredLoadingTest, Left) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; left:-10000px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadLeft, 1);
}

TEST_F(DeferredLoadingTest, AboveAndLeft) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; left:-10000px; top:-10000px' sandbox>"
      "</iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadAboveAndLeft, 1);
  histogramTester.expectTotalCount(kHistogramName, 2);
}

TEST_F(DeferredLoadingTest, ZeroByZero) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='height:0px;width:0px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadZeroByZero, 1);
}

TEST_F(DeferredLoadingTest, DisplayNone) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete("<iframe style='display:none' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadDisplayNone, 1);
}

TEST_F(DeferredLoadingTest, VisibleNestedInRight) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; left:105vw;' src='iframe.html' "
      "sandbox></iframe>");
  frameResource.complete("<iframe sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 2);
  histogramTester.expectTotalCount(kHistogramName, 2);
}

TEST_F(DeferredLoadingTest, LeftNestedInBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:105vh;' src='iframe.html' "
      "sandbox></iframe>");
  frameResource.complete(
      "<iframe style='position:absolute; left:-10000px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 2);
  histogramTester.expectTotalCount(kHistogramName, 2);
}

TEST_F(DeferredLoadingTest, SameOriginNotCounted) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete("<iframe src='iframe.html'></iframe>");
  frameResource.complete("<iframe></iframe>");
  compositeFrame();

  histogramTester.expectTotalCount(kHistogramName, 0);
}

}  // namespace blink
