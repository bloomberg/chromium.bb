// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

class MediaElementFillingViewportTest : public SimTest {
 protected:
  MediaElementFillingViewportTest() { webView().resize(WebSize(640, 480)); }

  bool isMostlyFillingViewport(HTMLMediaElement* element) {
    return element->m_mostlyFillingViewport;
  }

  bool viewportFillDebouncerTimerActive(HTMLMediaElement* element) {
    return element->m_viewportFillDebouncerTimer.isActive();
  }

  void checkViewportIntersectionChanged(HTMLMediaElement* element) {
    element->activateViewportIntersectionMonitoring(true);
    EXPECT_TRUE(element->m_checkViewportIntersectionTimer.isActive());
    // TODO(xjz): Mock the time and wait for 1s instead.
    element->checkViewportIntersectionTimerFired(nullptr);
  }

  std::unique_ptr<SimRequest> createMainResource() {
    std::unique_ptr<SimRequest> mainResource =
        WTF::wrapUnique(new SimRequest("https://example.com/", "text/html"));
    loadURL("https://example.com");
    return mainResource;
  }
};

TEST_F(MediaElementFillingViewportTest, MostlyFillingViewport) {
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  mainResource->complete(
      "<!DOCTYPE html>"
      "<html>"
      "<video id='video' style = 'position:fixed; left:0; top:0; width:100%; "
      "height:100%;'>"
      "source src='test.webm'"
      "</video>"
      "</html>");
  compositor().beginFrame();

  HTMLMediaElement* element =
      toElement<HTMLMediaElement>(document().getElementById("video"));
  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_TRUE(viewportFillDebouncerTimerActive(element));
  // TODO(xjz): Mock the time and check isMostlyFillingViewport() after 5s.
}

TEST_F(MediaElementFillingViewportTest, NotMostlyFillingViewport) {
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  mainResource->complete(
      "<!DOCTYPE html>"
      "<html>"
      "<video id='video' style = 'position:fixed; left:0; top:0; width:80%; "
      "height:80%;'>"
      "source src='test.webm'"
      "</video>"
      "</html>");
  compositor().beginFrame();

  HTMLMediaElement* element =
      toElement<HTMLMediaElement>(document().getElementById("video"));
  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_FALSE(viewportFillDebouncerTimerActive(element));
}

TEST_F(MediaElementFillingViewportTest, FillingViewportChanged) {
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  mainResource->complete(
      "<!DOCTYPE html>"
      "<html>"
      "<video id='video' style = 'position:fixed; left:0; top:0; width:100%; "
      "height:100%;'>"
      "source src='test.webm'"
      "</video>"
      "</html>");
  compositor().beginFrame();

  HTMLMediaElement* element =
      toElement<HTMLMediaElement>(document().getElementById("video"));
  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_TRUE(viewportFillDebouncerTimerActive(element));

  element->setAttribute("style",
                        "position:fixed; left:0; top:0; width:80%; height:80%;",
                        ASSERT_NO_EXCEPTION);
  compositor().beginFrame();

  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_FALSE(viewportFillDebouncerTimerActive(element));
}

TEST_F(MediaElementFillingViewportTest, LargeVideo) {
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  mainResource->complete(
      "<!DOCTYPE html>"
      "<html>"
      "<video id='video' style = 'position:fixed; left:0; top:0; width:200%; "
      "height:200%;'>"
      "source src='test.webm'"
      "</video>"
      "</html>");
  compositor().beginFrame();

  HTMLMediaElement* element =
      toElement<HTMLMediaElement>(document().getElementById("video"));
  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_TRUE(viewportFillDebouncerTimerActive(element));
}

TEST_F(MediaElementFillingViewportTest, VideoScrollOutHalf) {
  std::unique_ptr<SimRequest> mainResource = createMainResource();
  mainResource->complete(
      "<!DOCTYPE html>"
      "<html>"
      "<video id='video' style = 'position:fixed; left:0; top:0; width:100%; "
      "height:100%;'>"
      "source src='test.webm'"
      "</video>"
      "</html>");
  compositor().beginFrame();

  HTMLMediaElement* element =
      toElement<HTMLMediaElement>(document().getElementById("video"));
  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_TRUE(viewportFillDebouncerTimerActive(element));

  element->setAttribute(
      "style", "position:fixed; left:0; top:240px; width:100%; height:100%;",
      ASSERT_NO_EXCEPTION);
  compositor().beginFrame();
  checkViewportIntersectionChanged(element);
  EXPECT_FALSE(isMostlyFillingViewport(element));
  EXPECT_FALSE(viewportFillDebouncerTimerActive(element));
}

}  // namespace blink
