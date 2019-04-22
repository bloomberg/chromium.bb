// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ImagePaintTimingDetectorTest
    : public RenderingTest,
      private ScopedFirstContentfulPaintPlusPlusForTest {
  using CallbackQueue = std::queue<WebWidgetClient::ReportTimeCallback>;

 public:
  ImagePaintTimingDetectorTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedFirstContentfulPaintPlusPlusForTest(true),
        base_url_("http://www.test.com/") {}

  ~ImagePaintTimingDetectorTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetUp() override {
    RenderingTest::SetUp();
    RenderingTest::EnableCompositing();
    ReplaceCallBackQueue(GetPaintTimingDetector());
  }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  PaintTimingDetector& GetPaintTimingDetector() {
    return GetFrameView().GetPaintTimingDetector();
  }
  PaintTimingDetector& GetChildPaintTimingDetector() {
    return GetChildFrameView().GetPaintTimingDetector();
  }

  IntRect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect();
  }

  LocalFrameView& GetChildFrameView() { return *ChildFrame().View(); }

  void ReplaceCallBackQueue(PaintTimingDetector& detector) {
    detector.GetImagePaintTimingDetector()
        .notify_swap_time_override_for_testing_ =
        base::BindRepeating(&ImagePaintTimingDetectorTest::FakeNotifySwapTime,
                            base::Unretained(this));
  }
  ImageRecord* FindLargestPaintCandidate() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.FindLargestPaintCandidate();
  }

  ImageRecord* FindChildFrameLargestPaintCandidate() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.FindLargestPaintCandidate();
  }

  unsigned CountRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.visible_node_map_.size();
  }

  unsigned CountChildFrameRecords() {
    return GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.visible_node_map_.size();
  }

  size_t CountRankingSetRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.size_ordered_set_.size();
  }

  void Analyze() {
    return GetPaintTimingDetector().GetImagePaintTimingDetector().Analyze();
  }

  TimeTicks LargestPaintStoredResult() {
    ImageRecord* record = GetPaintTimingDetector()
                              .GetImagePaintTimingDetector()
                              .largest_image_paint_;
    return !record ? base::TimeTicks() : record->paint_time;
  }

  void UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() {
    UpdateAllLifecyclePhasesForTest();
    if (!callback_queue_.empty()) {
      InvokeCallback();
    }
  }

  void InvokeCallback() {
    DCHECK_GT(callback_queue_.size(), 0UL);
    std::move(callback_queue_.front())
        .Run(WebWidgetClient::SwapResult::kDidSwap, CurrentTimeTicks());
    callback_queue_.pop();
  }

  void SetImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLImageElement(element)->SetImageForTest(content);
  }

  void SetChildFrameImageAndPaint(AtomicString id, int width, int height) {
    DCHECK(ChildFrame().GetDocument());
    Element* element = ChildFrame().GetDocument()->getElementById(id);
    DCHECK(element);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLImageElement(element)->SetImageForTest(content);
  }

  void SetVideoImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    DCHECK(element);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLVideoElement(element)->SetImageForTest(content);
  }

  void SetSVGImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToSVGImageElement(element)->SetImageForTest(content);
  }

  void SimulateScroll() { GetPaintTimingDetector().NotifyScroll(kUserScroll); }

 private:
  void FakeNotifySwapTime(WebWidgetClient::ReportTimeCallback callback) {
    callback_queue_.push(std::move(callback));
  }
  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_resource =
        ImageResourceContent::CreateLoaded(
            StaticBitmapImage::Create(image).get());
    return original_image_resource;
  }

  CallbackQueue callback_queue_;
  std::string base_url_;
};

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_NoImage) {
  SetBodyInnerHTML(R"HTML(
    <div></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_OneImage) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25ul);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest,
       IgnoreImageUntilInvalidatedRectSizeNonZero) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountRecords(), 0u);
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountRecords(), 1u);
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_Largest) {
  SetBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="smaller"></img>
    <img id="medium"></img>
    <img id="larger"></img>
  )HTML");
  SetImageAndPaint("smaller", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25ul);

  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
}

TEST_F(ImagePaintTimingDetectorTest,
       LargestImagePaint_IgnoreThoseOutsideViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      img {
        position: fixed;
        top: -100px;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_IgnoreTheRemoved) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_NE(LargestPaintStoredResult(), base::TimeTicks());

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
  EXPECT_EQ(LargestPaintStoredResult(), base::TimeTicks());
}

TEST_F(ImagePaintTimingDetectorTest,
       LargestImagePaint_NodeRemovedBetweenRegistrationAndInvocation) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesForTest();

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));

  InvokeCallback();

  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest,
       LargestImagePaint_ReattachedNodeUseFirstPaint) {
  WTF::ScopedMockClock clock;
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
    </div>
  )HTML");
  HTMLImageElement* image = HTMLImageElement::Create(GetDocument());
  image->setAttribute("id", "target");
  GetDocument().getElementById("parent")->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  clock.Advance(TimeDelta::FromSecondsD(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time, base::TimeTicks() + TimeDelta::FromSecondsD(1));

  GetDocument().getElementById("parent")->RemoveChild(image);
  clock.Advance(TimeDelta::FromSecondsD(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);

  GetDocument().getElementById("parent")->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  clock.Advance(TimeDelta::FromSecondsD(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time, base::TimeTicks() + TimeDelta::FromSecondsD(1));
}

// This is to prove that a swap time is assigned only to nodes of the frame who
// register the swap time. In other words, swap time A should match frame A;
// swap time B should match frame B.
TEST_F(ImagePaintTimingDetectorTest, MatchSwapTimeToNodesOfDifferentFrames) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img height="5" width="5" id="smaller"></img>
      <img height="9" width="9" id="larger"></img>
    </div>
  )HTML");

  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhasesForTest();
  SetImageAndPaint("smaller", 5, 5);
  UpdateAllLifecyclePhasesForTest();
  InvokeCallback();
  // record1 is the larger.
  ImageRecord* record1 = FindLargestPaintCandidate();
  const base::TimeTicks record1Time = record1->paint_time;
  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("larger"));
  UpdateAllLifecyclePhasesForTest();
  InvokeCallback();
  // record2 is the smaller.
  ImageRecord* record2 = FindLargestPaintCandidate();
  EXPECT_NE(record1Time, record2->paint_time);
}

TEST_F(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateResultWhenLargestChanged) {
  TimeTicks time1 = CurrentTimeTicks();
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");
  SetImageAndPaint("target1", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  TimeTicks time2 = CurrentTimeTicks();
  TimeTicks result1 = LargestPaintStoredResult();
  EXPECT_GE(result1, time1);
  EXPECT_GE(time2, result1);

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  TimeTicks time3 = CurrentTimeTicks();
  TimeTicks result2 = LargestPaintStoredResult();
  EXPECT_GE(result2, time2);
  EXPECT_GE(time3, result2);
}

TEST_F(ImagePaintTimingDetectorTest, OneSwapPromiseForOneFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <div id="parent">
      <img id="1"></img>
      <img id="2"></img>
    </div>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  UpdateAllLifecyclePhasesForTest();

  SetImageAndPaint("2", 9, 9);
  UpdateAllLifecyclePhasesForTest();

  // This callback only assigns a time to the 5x5 image.
  InvokeCallback();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 81ul);
  EXPECT_TRUE(record->paint_time.is_null());

  // This callback assigns a time to the 9x9 image.
  InvokeCallback();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 81ul);
  EXPECT_FALSE(record->paint_time.is_null());
}

TEST_F(ImagePaintTimingDetectorTest, VideoImage) {
  SetBodyInnerHTML(R"HTML(
    <video id="target" poster="http://example.com/nonexistant.gif"></video>
  )HTML");

  SetVideoImageAndPaint("target", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0ul);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetBodyInnerHTML(R"HTML(
    <video id="target" poster="http://example.com/nonexistant.gif"></video>
  )HTML");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, SVGImage) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <image id="target" width="10" height="10"
        xlink:href="http://example.com/nonexistant.jpg"/>
    </svg>
  )HTML");

  SetSVGImageAndPaint("target", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0ul);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div>
      place-holder
    </div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountRecords(), 1u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
  )HTML");
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  SetBodyInnerHTML(R"HTML(
    <html>
    <style>
      html {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    </html>
  )HTML");
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreGradient) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        background-image: linear-gradient(blue, yellow);
      }
    </style>
    <div>
      place-holder
    </div>
  )HTML");
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, DeactivateAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateScroll();
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, NullTimeNoCrash) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesForTest();
  Analyze();
}

TEST_F(ImagePaintTimingDetectorTest, Iframe) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  ReplaceCallBackQueue(GetChildPaintTimingDetector());
  SetChildFrameImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesForTest();
  // Ensure main frame doesn't capture this image.
  EXPECT_EQ(CountRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  InvokeCallback();
  ImageRecord* image = FindChildFrameLargestPaintCandidate();
  EXPECT_TRUE(image);
  // Ensure the image size is not clipped (5*5).
  EXPECT_EQ(image->first_size, 25ul);
}

TEST_F(ImagePaintTimingDetectorTest, Iframe_ClippedByMainFrameViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f { margin-top: 1234567px }
    </style>
    <iframe id="f" width=100px height=100px></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  // Make sure the iframe is out of main-frame's viewport.
  DCHECK_LT(GetViewportRect(GetFrameView()).Height(), 1234567);
  ReplaceCallBackQueue(GetChildPaintTimingDetector());
  SetChildFrameImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, Iframe_HalfClippedByMainFrameViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f { margin-left: -5px; }
    </style>
    <iframe id="f" width=10px height=10px></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  ReplaceCallBackQueue(GetChildPaintTimingDetector());
  SetChildFrameImageAndPaint("target", 10, 10);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  InvokeCallback();
  ImageRecord* image = FindChildFrameLargestPaintCandidate();
  EXPECT_TRUE(image);
  EXPECT_LT(image->first_size, 100ul);
}

TEST_F(ImagePaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id='1'></img>
    <img id='2'></img>
    <img id='3'></img>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  SetImageAndPaint("2", 5, 5);
  SetImageAndPaint("3", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountRankingSetRecords(), 3u);
}

TEST_F(ImagePaintTimingDetectorTest, UseIntrinsicSizeIfSmaller_Image) {
  SetBodyInnerHTML(R"HTML(
    <img height="300" width="300" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25u);
}

TEST_F(ImagePaintTimingDetectorTest, NotUseIntrinsicSizeIfLarger_Image) {
  SetBodyInnerHTML(R"HTML(
    <img height="1" width="1" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 1u);
}

TEST_F(ImagePaintTimingDetectorTest,
       UseIntrinsicSizeIfSmaller_BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 50px;
        height: 50px;
        background-image: url("data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
      }
    </style>
    <div id="d"></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 1u);
}

TEST_F(ImagePaintTimingDetectorTest,
       NotUseIntrinsicSizeIfLarger_BackgroundImage) {
  // The image is in 16x16.
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 5px;
        height: 5px;
        background-image: url("data:image/gif;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAAb5JREFUOMulkr1KA0EQgGdvTwwnYmER0gQsrFKmSy+pLESw9Qm0F/ICNnba+h6iEOuAEWslKJKTOyJJvIT72d1xZuOFC0giOLA77O7Mt/PnNptN+I+49Xr9GhH3f3mb0v1ht9vtLAUYYw5ItkgDL3KyD8PhcLvdbl/WarXT3DjLMnAcR/f7/YfxeKwtgC5RKQVhGILWeg4hQ6hUKjWyucmhLFEUuWR3QYBWAZABQ9i5CCmXy16pVALP80BKaaG+70MQBLvzFMjRKKXh8j6FSYKF7ITdEWLa4/ktokN74wiqjSMpnVcbQZqmEJHz+ckeCPFjWKwULpyspAqhdXVXdcnZcPjsIgn+2BsVA8jVYuWlgJ3yBj0icgq2uoK+lg4t+ZvLomSKamSQ4AI5BcMADtMhyNoSgNIISUaFNtwlazcDcBc4gjjVwCWid2usCWroYEhnaqbzFJLUzAHIXRDChXCcQP8zhkSZ5eNLgHAUzwDcRu4CoIRn/wsGUQIIy4Vr9TH6SYFCNzw4nALn5627K4vIttOUOwfa5YnrDYzt/9OLv9I5l8kk5hZ3XLO20b7tbR7zHLy/BX8G0IeBEM7ZN1NGIaFUaKLgAAAAAElFTkSuQmCC");
      }
    </style>
    <div id="d"></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25u);
}

}  // namespace blink
