// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MediaElementFillingViewportTest : public SimTest {
 protected:
  MediaElementFillingViewportTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameWidget()->Resize(WebSize(640, 480));
  }

  bool IsMostlyFillingViewport(HTMLMediaElement* element) {
    return element->mostly_filling_viewport_;
  }

  void ActivateViewportIntersectionMonitoring(HTMLMediaElement* element,
                                              bool enable) {
    element->ActivateViewportIntersectionMonitoring(enable);
    EXPECT_EQ(enable, !!element->viewport_intersection_observer_);
  }

  void DoCompositeAndPropagate() {
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  std::unique_ptr<SimRequest> CreateMainResource() {
    std::unique_ptr<SimRequest> main_resource =
        std::make_unique<SimRequest>("https://example.com/", "text/html");
    LoadURL("https://example.com");
    return main_resource;
  }
};

TEST_F(MediaElementFillingViewportTest, MostlyFillingViewport) {
  std::unique_ptr<SimRequest> main_resource = CreateMainResource();
  main_resource->Complete(R"HTML(
    <!DOCTYPE html>
    <html>
    <video id='video' style = 'position:fixed; left:0; top:0; width:100%;
    height:100%;'>
    source src='test.webm'
    </video>
    </html>
  )HTML");
  Compositor().BeginFrame();

  HTMLMediaElement* element =
      ToElement<HTMLMediaElement>(GetDocument().getElementById("video"));

  ActivateViewportIntersectionMonitoring(element, true);
  DoCompositeAndPropagate();
  EXPECT_TRUE(IsMostlyFillingViewport(element));

  ActivateViewportIntersectionMonitoring(element, false);
  DoCompositeAndPropagate();
  EXPECT_FALSE(IsMostlyFillingViewport(element));
}

TEST_F(MediaElementFillingViewportTest, NotMostlyFillingViewport) {
  std::unique_ptr<SimRequest> main_resource = CreateMainResource();
  main_resource->Complete(R"HTML(
    <!DOCTYPE html>
    <html>
    <video id='video' style = 'position:fixed; left:0; top:0; width:80%;
    height:80%;'>
    source src='test.webm'
    </video>
    </html>
  )HTML");
  Compositor().BeginFrame();

  HTMLMediaElement* element =
      ToElement<HTMLMediaElement>(GetDocument().getElementById("video"));
  ActivateViewportIntersectionMonitoring(element, true);
  DoCompositeAndPropagate();
  EXPECT_FALSE(IsMostlyFillingViewport(element));
}

TEST_F(MediaElementFillingViewportTest, FillingViewportChanged) {
  std::unique_ptr<SimRequest> main_resource = CreateMainResource();
  main_resource->Complete(R"HTML(
    <!DOCTYPE html>
    <html>
    <video id='video' style = 'position:fixed; left:0; top:0; width:100%;
    height:100%;'>
    source src='test.webm'
    </video>
    </html>
  )HTML");
  Compositor().BeginFrame();

  HTMLMediaElement* element =
      ToElement<HTMLMediaElement>(GetDocument().getElementById("video"));

  ActivateViewportIntersectionMonitoring(element, true);
  DoCompositeAndPropagate();
  EXPECT_TRUE(IsMostlyFillingViewport(element));

  element->setAttribute("style",
                        "position:fixed; left:0; top:0; width:80%; height:80%;",
                        ASSERT_NO_EXCEPTION);
  DoCompositeAndPropagate();
  EXPECT_FALSE(IsMostlyFillingViewport(element));
}

TEST_F(MediaElementFillingViewportTest, LargeVideo) {
  std::unique_ptr<SimRequest> main_resource = CreateMainResource();
  main_resource->Complete(R"HTML(
    <!DOCTYPE html>
    <html>
    <video id='video' style = 'position:fixed; left:0; top:0; width:200%;
    height:200%;'>
    source src='test.webm'
    </video>
    </html>
  )HTML");
  Compositor().BeginFrame();

  HTMLMediaElement* element =
      ToElement<HTMLMediaElement>(GetDocument().getElementById("video"));

  ActivateViewportIntersectionMonitoring(element, true);
  DoCompositeAndPropagate();
  EXPECT_TRUE(IsMostlyFillingViewport(element));
}

TEST_F(MediaElementFillingViewportTest, VideoScrollOutHalf) {
  std::unique_ptr<SimRequest> main_resource = CreateMainResource();
  main_resource->Complete(R"HTML(
    <!DOCTYPE html>
    <html>
    <video id='video' style = 'position:fixed; left:0; top:0; width:100%;
    height:100%;'>
    source src='test.webm'
    </video>
    </html>
  )HTML");
  Compositor().BeginFrame();

  HTMLMediaElement* element =
      ToElement<HTMLMediaElement>(GetDocument().getElementById("video"));

  ActivateViewportIntersectionMonitoring(element, true);
  DoCompositeAndPropagate();
  EXPECT_TRUE(IsMostlyFillingViewport(element));

  element->setAttribute(
      "style", "position:fixed; left:0; top:240px; width:100%; height:100%;",
      ASSERT_NO_EXCEPTION);
  DoCompositeAndPropagate();
  EXPECT_FALSE(IsMostlyFillingViewport(element));
}

}  // namespace blink
