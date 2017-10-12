// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaCustomControlsFullscreenDetector.h"

#include "core/event_type_names.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntRect.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

struct VideoTestParam {
  String description;
  IntRect target_rect;
  bool expected_result;
};

}  // anonymous namespace

class MediaCustomControlsFullscreenDetectorTest
    : public ::testing::Test,
      private ScopedVideoFullscreenDetectionForTest {
 public:
  MediaCustomControlsFullscreenDetectorTest()
      : ScopedVideoFullscreenDetectionForTest(true) {}

 protected:
  void SetUp() override {

    page_holder_ = DummyPageHolder::Create();
    new_page_holder_ = DummyPageHolder::Create();
  }

  HTMLVideoElement* VideoElement() const {
    return ToHTMLVideoElement(GetDocument().QuerySelector("video"));
  }

  static MediaCustomControlsFullscreenDetector* FullscreenDetectorFor(
      HTMLVideoElement* video_element) {
    return video_element->custom_controls_fullscreen_detector_;
  }

  MediaCustomControlsFullscreenDetector* FullscreenDetector() const {
    return FullscreenDetectorFor(VideoElement());
  }

  Document& GetDocument() const { return page_holder_->GetDocument(); }
  Document& NewDocument() const { return new_page_holder_->GetDocument(); }

  bool CheckEventListenerRegistered(EventTarget& target,
                                    const AtomicString& event_type,
                                    EventListener* listener) {
    EventListenerVector* listeners = target.GetEventListeners(event_type);
    if (!listeners)
      return false;

    for (const auto& registered_listener : *listeners) {
      if (registered_listener.Callback() == listener)
        return true;
    }
    return false;
  }

  static bool ComputeIsDominantVideo(const IntRect& target_rect,
                                     const IntRect& root_rect,
                                     const IntRect& intersection_rect) {
    return MediaCustomControlsFullscreenDetector::
        ComputeIsDominantVideoForTests(target_rect, root_rect,
                                       intersection_rect);
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<DummyPageHolder> new_page_holder_;
  Persistent<HTMLVideoElement> video_;
};

TEST_F(MediaCustomControlsFullscreenDetectorTest, computeIsDominantVideo) {
  // TestWithParam cannot be applied here as IntRect needs the memory allocator
  // to be initialized, but the array of parameters is statically initialized,
  // which is before the memory allocation initialization.
  VideoTestParam test_params[] = {
      {"xCompleteFill", {0, 0, 100, 50}, true},
      {"yCompleteFill", {0, 0, 50, 100}, true},
      {"xyCompleteFill", {0, 0, 100, 100}, true},
      {"xIncompleteFillTooSmall", {0, 0, 84, 50}, false},
      {"yIncompleteFillTooSmall", {0, 0, 50, 84}, false},
      {"xIncompleteFillJustRight", {0, 0, 86, 50}, true},
      {"yIncompleteFillJustRight", {0, 0, 50, 86}, true},
      {"xVisibleProportionTooSmall", {-26, 0, 100, 100}, false},
      {"yVisibleProportionTooSmall", {0, -26, 100, 100}, false},
      {"xVisibleProportionJustRight", {-24, 0, 100, 100}, true},
      {"yVisibleProportionJustRight", {0, -24, 100, 100}, true},
  };

  IntRect root_rect(0, 0, 100, 100);

  for (const VideoTestParam& test_param : test_params) {
    const IntRect& target_rect = test_param.target_rect;
    IntRect intersection_rect = Intersection(target_rect, root_rect);
    EXPECT_EQ(test_param.expected_result,
              ComputeIsDominantVideo(target_rect, root_rect, intersection_rect))
        << test_param.description << " failed";
  }
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasNoListenersBeforeAddingToDocument) {
  HTMLVideoElement* video =
      ToHTMLVideoElement(GetDocument().createElement("video"));

  EXPECT_FALSE(CheckEventListenerRegistered(GetDocument(),
                                            EventTypeNames::fullscreenchange,
                                            FullscreenDetectorFor(video)));
  EXPECT_FALSE(CheckEventListenerRegistered(
      GetDocument(), EventTypeNames::webkitfullscreenchange,
      FullscreenDetectorFor(video)));
  EXPECT_FALSE(CheckEventListenerRegistered(
      *video, EventTypeNames::loadedmetadata, FullscreenDetectorFor(video)));
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasListenersAfterAddToDocumentByScript) {
  HTMLVideoElement* video =
      ToHTMLVideoElement(GetDocument().createElement("video"));
  GetDocument().body()->AppendChild(video);

  EXPECT_TRUE(CheckEventListenerRegistered(
      GetDocument(), EventTypeNames::fullscreenchange, FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(
      GetDocument(), EventTypeNames::webkitfullscreenchange,
      FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(
      *VideoElement(), EventTypeNames::loadedmetadata, FullscreenDetector()));
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasListenersAfterAddToDocumentByParser) {
  GetDocument().body()->SetInnerHTMLFromString("<body><video></video></body>");

  EXPECT_TRUE(CheckEventListenerRegistered(
      GetDocument(), EventTypeNames::fullscreenchange, FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(
      GetDocument(), EventTypeNames::webkitfullscreenchange,
      FullscreenDetector()));
  EXPECT_TRUE(CheckEventListenerRegistered(
      *VideoElement(), EventTypeNames::loadedmetadata, FullscreenDetector()));
}

TEST_F(MediaCustomControlsFullscreenDetectorTest,
       hasListenersAfterDocumentMove) {
  HTMLVideoElement* video =
      ToHTMLVideoElement(GetDocument().createElement("video"));
  GetDocument().body()->AppendChild(video);

  NewDocument().body()->AppendChild(VideoElement());

  EXPECT_FALSE(CheckEventListenerRegistered(GetDocument(),
                                            EventTypeNames::fullscreenchange,
                                            FullscreenDetectorFor(video)));
  EXPECT_FALSE(CheckEventListenerRegistered(
      GetDocument(), EventTypeNames::webkitfullscreenchange,
      FullscreenDetectorFor(video)));

  EXPECT_TRUE(CheckEventListenerRegistered(NewDocument(),
                                           EventTypeNames::fullscreenchange,
                                           FullscreenDetectorFor(video)));
  EXPECT_TRUE(CheckEventListenerRegistered(
      NewDocument(), EventTypeNames::webkitfullscreenchange,
      FullscreenDetectorFor(video)));

  EXPECT_TRUE(CheckEventListenerRegistered(
      *video, EventTypeNames::loadedmetadata, FullscreenDetectorFor(video)));
}

}  // namespace blink
