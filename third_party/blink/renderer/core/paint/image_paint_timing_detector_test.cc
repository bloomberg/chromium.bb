// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/paint/paint_tracker.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ImagePaintTimingDetectorTest
    : public PageTestBase,
      private ScopedFirstContentfulPaintPlusPlusForTest {
  using CallbackQueue = std::queue<WebLayerTreeView::ReportTimeCallback>;

 public:
  ImagePaintTimingDetectorTest()
      : ScopedFirstContentfulPaintPlusPlusForTest(true),
        base_url_("http://www.test.com/"){};

  ~ImagePaintTimingDetectorTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetUp() override {
    PageTestBase::SetUp();
    GetPaintTracker()
        .GetImagePaintTimingDetector()
        .notify_swap_time_override_for_testing_ =
        base::BindRepeating(&ImagePaintTimingDetectorTest::FakeNotifySwapTime,
                            base::Unretained(this));
  }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  PaintTracker& GetPaintTracker() { return GetFrameView().GetPaintTracker(); }
  ImageRecord* FindLargestPaintCandidate() {
    return GetPaintTracker()
        .GetImagePaintTimingDetector()
        .FindLargestPaintCandidate();
  }

  ImageRecord* FindLastPaintCandidate() {
    return GetPaintTracker()
        .GetImagePaintTimingDetector()
        .FindLastPaintCandidate();
  }

  unsigned CountRecords() {
    return GetPaintTracker()
        .GetImagePaintTimingDetector()
        .id_record_map_.size();
  }

  TimeTicks LargestPaintStoredResult() {
    return GetPaintTracker().GetImagePaintTimingDetector().largest_image_paint_;
  }

  TimeTicks LastPaintStoredResult() {
    return GetPaintTracker().GetImagePaintTimingDetector().last_image_paint_;
  }

  void UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() {
    UpdateAllLifecyclePhasesForTest();
    if (callback_queue_.size() > 0) {
      InvokeCallback();
    }
  }

  void InvokeCallback() {
    DCHECK_GT(callback_queue_.size(), 0UL);
    std::move(callback_queue_.front())
        .Run(WebLayerTreeView::SwapResult::kDidSwap, CurrentTimeTicks());
    callback_queue_.pop();
  }

  void SetImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLImageElement(element)->SetImageForTest(content);
  }

  void SetVideoImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
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

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

 private:
  void FakeNotifySwapTime(WebLayerTreeView::ReportTimeCallback callback) {
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
  UpdateAllLifecyclePhasesForTest();
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
  EXPECT_EQ(record->first_size, 25);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_Largest) {
  SetBodyInnerHTML(R"HTML(
    <img id="smaller"></img>
    <img id="medium"></img>
    <img id="larger"></img>
  )HTML");
  SetImageAndPaint("smaller", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25);

  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
#if defined(OS_MACOSX)
  EXPECT_EQ(record->first_size, 90);
#else
  EXPECT_EQ(record->first_size, 81);
#endif
  EXPECT_TRUE(record->loaded);

  SetImageAndPaint("medium", 7, 7);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
#if defined(OS_MACOSX)
  EXPECT_EQ(record->first_size, 90);
#else
  EXPECT_EQ(record->first_size, 81);
#endif
  EXPECT_TRUE(record->loaded);
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

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_IgnoreReAttached) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
    </div>
  )HTML");
  HTMLImageElement* image = HTMLImageElement::Create(GetDocument());
  image->setAttribute("id", "target");
  GetDocument().getElementById("parent")->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);

  GetDocument().getElementById("parent")->RemoveChild(image);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);

  GetDocument().getElementById("parent")->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
}

// This test dipicts a situation when a smaller image has loaded, but a larger
// image is loading. When we call analyze, the result will be empty because
// we don't know when the largest image will finish loading. We wait until
// next analysis to make the judgement again.
// This bahavior is the same with Last Image Paint as well.
TEST_F(ImagePaintTimingDetectorTest, DiscardAnalysisWhenLargestIsLoading) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img height="5" width="5" id="1"></img>
      <img height="9" width="9" id="2"></img>
    </div>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  UpdateAllLifecyclePhasesForTest();
  ImageRecord* record;
  InvokeCallback();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);

  SetImageAndPaint("2", 9, 9);
  UpdateAllLifecyclePhasesForTest();
  InvokeCallback();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
#if defined(OS_MACOSX)
  EXPECT_EQ(record->first_size, 90);
#else
  EXPECT_EQ(record->first_size, 81);
#endif
  EXPECT_FALSE(record->first_paint_time_after_loaded.is_null());
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
  const base::TimeTicks record1Time = record1->first_paint_time_after_loaded;
  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("larger"));
  UpdateAllLifecyclePhasesForTest();
  InvokeCallback();
  // record2 is the smaller.
  ImageRecord* record2 = FindLargestPaintCandidate();
  EXPECT_NE(record1Time, record2->first_paint_time_after_loaded);
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

TEST_F(ImagePaintTimingDetectorTest, LastImagePaint_NoImage) {
  SetBodyInnerHTML(R"HTML(
    <div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  ImageRecord* record = FindLastPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, LastImagePaint_OneImage) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");

  SetImageAndPaint("target", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, LastImagePaint_Last) {
  WTF::ScopedMockClock clock;
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img height="10" width="10" id="1"></img>
      <img height="5" width="5" id="2"></img>
      <img height="7" width="7" id="3"></img>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  SetImageAndPaint("1", 10, 10);
  UpdateAllLifecyclePhasesForTest();
  clock.Advance(TimeDelta::FromSecondsD(1));
  InvokeCallback();

  ImageRecord* record;
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 100);
  EXPECT_EQ(record->first_paint_time_after_loaded,
            base::TimeTicks() + TimeDelta::FromSecondsD(1));

  SetImageAndPaint("2", 5, 5);
  UpdateAllLifecyclePhasesForTest();
  clock.Advance(TimeDelta::FromSecondsD(1));
  InvokeCallback();

  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
#if defined(OS_MACOSX)
  EXPECT_EQ(record->first_size, 30);
#else
  EXPECT_EQ(record->first_size, 25);
#endif
  EXPECT_EQ(record->first_paint_time_after_loaded,
            base::TimeTicks() + TimeDelta::FromSecondsD(2));

  SetImageAndPaint("3", 7, 7);
  UpdateAllLifecyclePhasesForTest();
  clock.Advance(TimeDelta::FromSecondsD(1));
  // 6th s
  InvokeCallback();
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GE(record->first_paint_time_after_loaded,
            base::TimeTicks() + TimeDelta::FromSecondsD(3));

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("3"));
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GE(record->first_paint_time_after_loaded,
            base::TimeTicks() + TimeDelta::FromSecondsD(2));
}

TEST_F(ImagePaintTimingDetectorTest, LastImagePaint_LastBasedOnLoadTime) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img height="5" width="5" id="1"></img>
    </div>
  )HTML");
  Element* image = GetDocument().CreateRawElement(html_names::kImgTag);
  image->setAttribute(html_names::kIdAttr, "2");
  image->setAttribute(html_names::kHeightAttr, "10");
  image->setAttribute(html_names::kWidthAttr, "10");
  GetDocument().getElementById("parent")->appendChild(image);
  SetImageAndPaint("2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();

  SetImageAndPaint("1", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();

  ImageRecord* record;
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25);
}

TEST_F(ImagePaintTimingDetectorTest, LastImagePaint_IgnoreTheRemoved) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLastPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest,
       LastImagePaint_IgnoreThoseOutsideViewport) {
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
  ImageRecord* record = FindLastPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, LastImagePaint_OneSwapPromiseForOneFrame) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="1"></img>
      <img id="2"></img>
    </div>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  UpdateAllLifecyclePhasesForTest();

  SetImageAndPaint("2", 9, 9);
  UpdateAllLifecyclePhasesForTest();

  InvokeCallback();
  ImageRecord* record;
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
#if defined(OS_MACOSX)
  EXPECT_EQ(record->first_size, 90);
#else
  EXPECT_EQ(record->first_size, 81);
#endif
  EXPECT_TRUE(record->first_paint_time_after_loaded.is_null());

  InvokeCallback();
  record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
#if defined(OS_MACOSX)
  EXPECT_EQ(record->first_size, 90);
#else
  EXPECT_EQ(record->first_size, 81);
#endif
  EXPECT_FALSE(record->first_paint_time_after_loaded.is_null());
}

TEST_F(ImagePaintTimingDetectorTest,
       LastImagePaint_UpdateResultWhenLastChanged) {
  TimeTicks time1 = CurrentTimeTicks();
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target1"></img>
    </div>
  )HTML");
  SetImageAndPaint("target1", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  TimeTicks time2 = CurrentTimeTicks();
  TimeTicks result1 = LastPaintStoredResult();
  EXPECT_GE(result1, time1);
  EXPECT_GE(time2, result1);

  Element* image = GetDocument().CreateRawElement(html_names::kImgTag);
  image->setAttribute(html_names::kIdAttr, "target2");
  GetDocument().getElementById("parent")->appendChild(image);
  SetImageAndPaint("target2", 2, 2);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  TimeTicks time3 = CurrentTimeTicks();
  TimeTicks result2 = LastPaintStoredResult();
  EXPECT_GE(result2, time2);
  EXPECT_GE(time3, result2);
}

TEST_F(ImagePaintTimingDetectorTest, VideoImage) {
  SetBodyInnerHTML(R"HTML(
    <video id="target" poster="http://example.com/nonexistant.gif"></video>
  )HTML");

  SetVideoImageAndPaint("target", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetBodyInnerHTML(R"HTML(
    <video id="target" poster="http://example.com/nonexistant.gif"></video>
  )HTML");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLastPaintCandidate();
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
  ImageRecord* record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage) {
  RegisterMockedHttpURLLoad("white-1x1.png");
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        background-image: url('white-1x1.png');
      }
    </style>
    <div>
      place-holder
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  ImageRecord* record = FindLastPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountRecords(), 1u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  RegisterMockedHttpURLLoad("white-1x1.png");
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        background-image: url('white-1x1.png');
      }
    </style>
    <body>
    </body>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  RegisterMockedHttpURLLoad("white-1x1.png");
  SetBodyInnerHTML(R"HTML(
    <html>
    <style>
      html {
        background-image: url('white-1x1.png');
      }
    </style>
    </html>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
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
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountRecords(), 0u);
}

}  // namespace blink
