// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/HistogramTester.h"
#include "platform/testing/UnitTestHelpers.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

static const char* kHistogramName =
    "Navigation.DeferredDocumentLoading.StatesV4";

class DeferredLoadingTest : public SimTest {
 protected:
  DeferredLoadingTest() { webView().resize(WebSize(640, 480)); }
  void compositeFrame() {
    while (compositor().needsBeginFrame()) {
      compositor().beginFrame();
      testing::runPendingTasks();
    }
  }

  std::unique_ptr<SimRequest> createMainResource() {
    std::unique_ptr<SimRequest> mainResource =
        WTF::wrapUnique(new SimRequest("https://example.com/", "text/html"));
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
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
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

TEST_F(DeferredLoadingTest, TwoScreensBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; top:205vh;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 3);
}

TEST_F(DeferredLoadingTest, Above) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; top:-10000px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

TEST_F(DeferredLoadingTest, Left) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; left:-10000px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

TEST_F(DeferredLoadingTest, AboveAndLeft) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; left:-10000px; top:-10000px' sandbox>"
      "</iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

TEST_F(DeferredLoadingTest, ZeroByZero) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='height:0px;width:0px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
}

TEST_F(DeferredLoadingTest, DisplayNone) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete("<iframe style='display:none' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadNoParent, 1);
  histogramTester.expectTotalCount(kHistogramName, 6);
}

TEST_F(DeferredLoadingTest, DisplayNoneIn2ScreensBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:205vh' "
      "src='iframe.html' sandbox></iframe>");
  frameResource.complete("<iframe style='display:none' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadNoParent, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 2);
  histogramTester.expectTotalCount(kHistogramName, 9);
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
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 2);
  histogramTester.expectTotalCount(kHistogramName, 8);
}

TEST_F(DeferredLoadingTest, OneScreenBelowThenScriptedVisible) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->start();
  mainResource->write(
      "<iframe id='theFrame' style='position:absolute; top:105vh;' "
      "sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectTotalCount(kHistogramName, 4);

  mainResource->write("<script>theFrame.style.top='10px'</script>");
  mainResource->finish();

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

TEST_F(DeferredLoadingTest, OneScreenBelowThenScrolledVisible) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe id='theFrame' style='position:absolute; top:105vh; height:10px' "
      "sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 4);

  mainFrame().setScrollOffset(WebSize(0, 50));

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

TEST_F(DeferredLoadingTest, DisplayNoneThenTwoScreensAway) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->start();
  mainResource->write(
      "<iframe id='theFrame' style='display:none' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectTotalCount(kHistogramName, 6);

  mainResource->write(
      "<script>theFrame.style.top='200vh';"
      "theFrame.style.position='absolute';"
      "theFrame.style.display='block';</script>");
  mainResource->finish();

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, WouldLoadNoParent, 1);
  histogramTester.expectTotalCount(kHistogramName, 6);
}

TEST_F(DeferredLoadingTest, DisplayNoneAsync) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->start();
  mainResource->write("some stuff");

  compositeFrame();

  mainResource->write(
      "<script>frame = document.createElement('iframe');"
      "frame.setAttribute('sandbox', true);"
      "frame.style.display = 'none';"
      "document.body.appendChild(frame);"
      "</script>");
  mainResource->finish();

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadNoParent, 1);
  histogramTester.expectTotalCount(kHistogramName, 6);
}

TEST_F(DeferredLoadingTest, TwoScreensAwayThenDisplayNoneThenNew) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->start();
  mainResource->write(
      "<iframe id='theFrame' style='position:absolute; top:205vh' sandbox>"
      "</iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 3);

  mainResource->write("<script>theFrame.style.display='none'</script>");

  compositeFrame();

  histogramTester.expectTotalCount(kHistogramName, 6);

  mainResource->write(
      "<script>document.body.appendChild(document.createElement"
      "('iframe'));</script>");
  mainResource->finish();

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadNoParent, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 6);
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

TEST_F(DeferredLoadingTest, AboveNestedInThreeScreensBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:300vh' src='iframe.html' "
      "sandbox></iframe>");
  frameResource.complete(
      "<iframe style='position:absolute; top:-10000px;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 2);
  histogramTester.expectTotalCount(kHistogramName, 4);
}

TEST_F(DeferredLoadingTest, VisibleNestedInTwoScreensBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:205vh' src='iframe.html' "
      "sandbox></iframe>");
  frameResource.complete("<iframe sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 2);
  histogramTester.expectTotalCount(kHistogramName, 6);
}

TEST_F(DeferredLoadingTest, ThreeScreensBelowNestedInTwoScreensBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:205vh' src='iframe.html' "
      "sandbox></iframe>");
  frameResource.complete(
      "<iframe style='position:absolute; top:305vh' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 4);
}

TEST_F(DeferredLoadingTest, TriplyNested) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");
  SimRequest frameResource2("https://example.com/iframe2.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:300vh' src='iframe.html' "
      "sandbox></iframe>");
  frameResource.complete(
      "<iframe style='position:absolute; top:200vh' src='iframe2.html' "
      "sandbox></iframe>");
  frameResource2.complete(
      "<iframe style='position:absolute; top:100vh' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 3);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 4);
}

TEST_F(DeferredLoadingTest, NestedFramesOfVariousSizes) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");
  SimRequest frameResource2("https://example.com/iframe2.html", "text/html");
  SimRequest frameResource3("https://example.com/iframe3.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:50vh; height:10px;'"
      "src='iframe.html' sandbox></iframe>");
  frameResource.complete(
      "<iframe style='position:absolute; top:200vh; height:100px;'"
      "src='iframe2.html' sandbox></iframe>");
  frameResource2.complete(
      "<iframe style='position:absolute; top:100vh; height:50px;'"
      "src='iframe3.html' sandbox></iframe>");
  frameResource3.complete(
      "<iframe style='position:absolute; top:100vh' sandbox></iframe>");
  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 4);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 2);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 3);
  histogramTester.expectTotalCount(kHistogramName, 11);
}

TEST_F(DeferredLoadingTest, FourScreensBelow) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; top:405vh;' sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectTotalCount(kHistogramName, 1);
}

TEST_F(DeferredLoadingTest, TallIFrameStartsAbove) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; top:-150vh; height:200vh;' sandbox>"
      "</iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

TEST_F(DeferredLoadingTest, OneDownAndOneRight) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();

  mainResource->complete(
      "<iframe style='position:absolute; left:100vw; top:100vh' sandbox>"
      "</iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectTotalCount(kHistogramName, 1);
}

TEST_F(DeferredLoadingTest, VisibleCrossOriginNestedInBelowFoldSameOrigin) {
  HistogramTester histogramTester;
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  mainResource->complete(
      "<iframe style='position:absolute; top:105vh' src='iframe.html'>"
      "</iframe>");
  frameResource.complete("<iframe sandbox></iframe>");

  compositeFrame();

  histogramTester.expectBucketCount(kHistogramName, Created, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoadVisible, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad1ScreenAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad2ScreensAway, 1);
  histogramTester.expectBucketCount(kHistogramName, WouldLoad3ScreensAway, 1);
  histogramTester.expectTotalCount(kHistogramName, 5);
}

}  // namespace blink
