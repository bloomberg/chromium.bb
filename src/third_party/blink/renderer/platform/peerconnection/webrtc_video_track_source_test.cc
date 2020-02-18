// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::_;
using testing::Invoke;
using testing::Mock;

namespace blink {

void ExpectUpdateRectEquals(const gfx::Rect& expected,
                            const webrtc::VideoFrame::UpdateRect actual) {
  EXPECT_EQ(expected.x(), actual.offset_x);
  EXPECT_EQ(expected.y(), actual.offset_y);
  EXPECT_EQ(expected.width(), actual.width);
  EXPECT_EQ(expected.height(), actual.height);
}

class MockVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  MOCK_METHOD1(OnFrame, void(const webrtc::VideoFrame&));
};

class WebRtcVideoTrackSourceTest
    : public ::testing::TestWithParam<media::VideoFrame::StorageType> {
 public:
  WebRtcVideoTrackSourceTest()
      : track_source_(new rtc::RefCountedObject<WebRtcVideoTrackSource>(
            /*is_screencast=*/false,
            /*needs_denoising=*/absl::nullopt)) {
    track_source_->AddOrUpdateSink(&mock_sink_, rtc::VideoSinkWants());
  }
  ~WebRtcVideoTrackSourceTest() override {
    track_source_->RemoveSink(&mock_sink_);
  }

  void SendTestFrame(const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     const gfx::Size& natural_size,
                     media::VideoFrame::StorageType storage_type) {
    scoped_refptr<media::VideoFrame> frame =
        CreateTestFrame(coded_size, visible_rect, natural_size, storage_type);
    track_source_->OnFrameCaptured(frame);
  }

  void SendTestFrameWithUpdateRect(
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      int capture_counter,
      const gfx::Rect& update_rect,
      media::VideoFrame::StorageType storage_type) {
    scoped_refptr<media::VideoFrame> frame =
        CreateTestFrame(coded_size, visible_rect, natural_size, storage_type);
    frame->metadata()->SetInteger(media::VideoFrameMetadata::CAPTURE_COUNTER,
                                  capture_counter);
    frame->metadata()->SetRect(media::VideoFrameMetadata::CAPTURE_UPDATE_RECT,
                               update_rect);
    track_source_->OnFrameCaptured(frame);
  }

  WebRtcVideoTrackSource::FrameAdaptationParams FrameAdaptation_KeepAsIs(
      const gfx::Size& natural_size) {
    return WebRtcVideoTrackSource::FrameAdaptationParams{
        false /*should_drop_frame*/,
        0 /*crop_x*/,
        0 /*crop_y*/,
        natural_size.width() /*crop_width*/,
        natural_size.height() /*crop_height*/,
        natural_size.width() /*scale_to_width*/,
        natural_size.height() /*scale_to_height*/
    };
  }

  WebRtcVideoTrackSource::FrameAdaptationParams FrameAdaptation_DropFrame() {
    return WebRtcVideoTrackSource::FrameAdaptationParams{
        true /*should_drop_frame*/,
        0 /*crop_x*/,
        0 /*crop_y*/,
        0 /*crop_width*/,
        0 /*crop_height*/,
        0 /*scale_to_width*/,
        0 /*scale_to_height*/
    };
  }

  WebRtcVideoTrackSource::FrameAdaptationParams FrameAdaptation_Scale(
      const gfx::Size& natural_size,
      const gfx::Size& scale_to_size) {
    return WebRtcVideoTrackSource::FrameAdaptationParams{
        false /*should_drop_frame*/,
        0 /*crop_x*/,
        0 /*crop_y*/,
        natural_size.width() /*crop_width*/,
        natural_size.height() /*crop_height*/,
        scale_to_size.width() /*scale_to_width*/,
        scale_to_size.height() /*scale_to_height*/
    };
  }

 protected:
  MockVideoSink mock_sink_;
  scoped_refptr<WebRtcVideoTrackSource> track_source_;
};

TEST_P(WebRtcVideoTrackSourceTest, CropFrameTo640360) {
  const gfx::Size kCodedSize(640, 480);
  const gfx::Rect kVisibleRect(0, 60, 640, 360);
  const gfx::Size kNaturalSize(640, 360);
  const media::VideoFrame::StorageType storage_type = GetParam();
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kNaturalSize](const webrtc::VideoFrame& frame) {
        EXPECT_EQ(kNaturalSize.width(), frame.width());
        EXPECT_EQ(kNaturalSize.height(), frame.height());
      }));
  SendTestFrame(kCodedSize, kVisibleRect, kNaturalSize, storage_type);
}

TEST_P(WebRtcVideoTrackSourceTest, CropFrameTo320320) {
  const gfx::Size kCodedSize(640, 480);
  const gfx::Rect kVisibleRect(80, 0, 480, 480);
  const gfx::Size kNaturalSize(320, 320);
  const media::VideoFrame::StorageType storage_type = GetParam();
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kNaturalSize](const webrtc::VideoFrame& frame) {
        EXPECT_EQ(kNaturalSize.width(), frame.width());
        EXPECT_EQ(kNaturalSize.height(), frame.height());
      }));
  SendTestFrame(kCodedSize, kVisibleRect, kNaturalSize, storage_type);
}

TEST_P(WebRtcVideoTrackSourceTest, Scale720To640360) {
  const gfx::Size kCodedSize(1280, 720);
  const gfx::Rect kVisibleRect(0, 0, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  const media::VideoFrame::StorageType storage_type = GetParam();
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kNaturalSize](const webrtc::VideoFrame& frame) {
        EXPECT_EQ(kNaturalSize.width(), frame.width());
        EXPECT_EQ(kNaturalSize.height(), frame.height());
      }));
  SendTestFrame(kCodedSize, kVisibleRect, kNaturalSize, storage_type);
}

TEST_P(WebRtcVideoTrackSourceTest, UpdateRectWithNoTransform) {
  const gfx::Size kCodedSize(640, 480);
  const gfx::Rect kVisibleRect(0, 0, 640, 480);
  const gfx::Size kNaturalSize(640, 480);
  const media::VideoFrame::StorageType storage_type = GetParam();
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  // Any UPDATE_RECT for the first received frame is expected to get
  // ignored and the full frame should be marked as updated.
  const gfx::Rect kUpdateRect1(1, 2, 3, 4);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  int capture_counter = 101;  // arbitrary absolute value
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect for second frame should get passed along.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kUpdateRect1](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(kUpdateRect1, frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Simulate the next frame getting dropped
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_DropFrame());
  const gfx::Rect kUpdateRect2(2, 3, 4, 5);
  EXPECT_CALL(mock_sink_, OnFrame(_)).Times(0);
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect2, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // The |update_rect| for the next frame is expected to contain the union
  // of the current an previous |update_rects|.
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));
  const gfx::Rect kUpdateRect3(3, 4, 5, 6);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(
          Invoke([kUpdateRect2, kUpdateRect3](const webrtc::VideoFrame& frame) {
            gfx::Rect expected_update_rect(kUpdateRect2);
            expected_update_rect.Union(kUpdateRect3);
            ExpectUpdateRectEquals(expected_update_rect, frame.update_rect());
          }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect3, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Simulate a gap in |capture_counter|. This is expected to cause the whole
  // frame to get marked as updated.
  ++capture_counter;
  const gfx::Rect kUpdateRect4(4, 5, 6, 7);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kVisibleRect](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(kVisibleRect, frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect4, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Important edge case (expected to be fairly common): An empty update rect
  // indicates that nothing has changed.
  const gfx::Rect kEmptyRectWithZeroOrigin(0, 0, 0, 0);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kEmptyRectWithZeroOrigin,
                              storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  const gfx::Rect kEmptyRectWithNonZeroOrigin(10, 20, 0, 0);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kEmptyRectWithNonZeroOrigin,
                              storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // A frame without a CAPTURE_COUNTER and CAPTURE_UPDATE_RECT is treated as the
  // whole content having changed.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kVisibleRect](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(kVisibleRect, frame.update_rect());
      }));
  SendTestFrame(kCodedSize, kVisibleRect, kNaturalSize, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);
}

TEST_P(WebRtcVideoTrackSourceTest, UpdateRectWithCropFromUpstream) {
  const gfx::Size kCodedSize(640, 480);
  const gfx::Rect kVisibleRect(100, 50, 200, 80);
  const gfx::Size kNaturalSize = gfx::Size(200, 80);
  const media::VideoFrame::StorageType storage_type = GetParam();
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  // Any UPDATE_RECT for the first received frame is expected to get
  // ignored and the full frame should be marked as updated.
  const gfx::Rect kUpdateRect1(120, 70, 160, 40);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  int capture_counter = 101;  // arbitrary absolute value
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect for second frame should get passed along.
  // Update rect fully contained in crop region.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(
          Invoke([kUpdateRect1, kVisibleRect](const webrtc::VideoFrame& frame) {
            gfx::Rect expected_update_rect(kUpdateRect1);
            expected_update_rect.Offset(-kVisibleRect.x(), -kVisibleRect.y());
            ExpectUpdateRectEquals(expected_update_rect, frame.update_rect());
          }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect outside crop region.
  const gfx::Rect kUpdateRect2(2, 3, 4, 5);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect2, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect partly overlapping crop region.
  const gfx::Rect kUpdateRect3(kVisibleRect.x() + 10, kVisibleRect.y() + 8,
                               kVisibleRect.width(), kVisibleRect.height());
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kVisibleRect](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(10, 8, kVisibleRect.width() - 10,
                                         kVisibleRect.height() - 8),
                               frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect3, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // When crop origin changes, the whole frame is expected to be marked as
  // changed.
  const gfx::Rect kVisibleRect2(kVisibleRect.x() + 1, kVisibleRect.y(),
                                kVisibleRect.width(), kVisibleRect.height());
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect2, kNaturalSize,
                              ++capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // When crop size changes, the whole frame is expected to be marked as
  // changed.
  const gfx::Rect kVisibleRect3(kVisibleRect2.x(), kVisibleRect2.y(),
                                kVisibleRect2.width(),
                                kVisibleRect2.height() - 1);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect3, kNaturalSize,
                              ++capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);
}

TEST_P(WebRtcVideoTrackSourceTest, UpdateRectWithScaling) {
  const gfx::Size kCodedSize(640, 480);
  const gfx::Rect kVisibleRect(100, 50, 200, 80);
  const gfx::Size kNaturalSize = gfx::Size(200, 80);
  const gfx::Size kScaleToSize = gfx::Size(120, 50);
  const media::VideoFrame::StorageType storage_type = GetParam();
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_Scale(kNaturalSize, kScaleToSize));

  // Any UPDATE_RECT for the first received frame is expected to get
  // ignored and no update rect should be set.
  const gfx::Rect kUpdateRect1(120, 70, 160, 40);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_FALSE(frame.has_update_rect());
      }));
  int capture_counter = 101;  // arbitrary absolute value
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              capture_counter, kUpdateRect1, storage_type);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // When scaling is applied and UPDATE_RECT is not empty, we scale the
  // update rect.
  // Calculated by hand according to KNaturalSize and KScaleToSize.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(10, 10, 100, 30), frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, kUpdateRect1, storage_type);

  // When UPDATE_RECT is empty, we expect to deliver an empty UpdateRect even if
  // scaling is applied.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, gfx::Rect(), storage_type);

  // When UPDATE_RECT is empty, but the scaling has changed, we expect to
  // deliver no known update_rect.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_FALSE(frame.has_update_rect());
      }));
  const gfx::Size kScaleToSize2 = gfx::Size(60, 26);
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_Scale(kNaturalSize, kScaleToSize2));
  SendTestFrameWithUpdateRect(kCodedSize, kVisibleRect, kNaturalSize,
                              ++capture_counter, gfx::Rect(), storage_type);

  Mock::VerifyAndClearExpectations(&mock_sink_);
}

INSTANTIATE_TEST_SUITE_P(
    WebRtcVideoTrackSourceTest,
    WebRtcVideoTrackSourceTest,
    testing::Values(media::VideoFrame::STORAGE_OWNED_MEMORY,
                    media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER));

}  // namespace blink
