// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "services/video_capture/test/fake_device_test.h"
#include "services/video_capture/test/mock_receiver.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace {

struct FrameInfo {
  gfx::Size size;
  media::VideoPixelFormat pixel_format;
  media::VideoPixelStorage storage_type;
  base::TimeDelta timestamp;
};

}  // anonymous namespace

namespace video_capture {

// This alias ensures test output is easily attributed to this service's tests.
// TODO(rockot/chfremer): Consider just renaming the type.
using FakeVideoCaptureDeviceTest = FakeDeviceTest;

TEST_F(FakeVideoCaptureDeviceTest, FrameCallbacksArrive) {
  base::RunLoop wait_loop;
  // Constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBufferHandle(_, _)).Times(AtLeast(1));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        num_frames_arrived += 1;
        if (num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));

  fake_device_proxy_->Start(requestable_settings_, std::move(receiver_proxy));
  wait_loop.Run();
}

// Tests that frames received from a fake capture device match the requested
// format and have increasing timestamps.
TEST_F(FakeVideoCaptureDeviceTest, ReceiveFramesFromFakeCaptureDevice) {
  base::RunLoop wait_loop;
  mojom::ReceiverPtr receiver_proxy;
  // Constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int num_frames_to_receive = 2;
  FrameInfo received_frame_infos[num_frames_to_receive];
  int received_frame_count = 0;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBufferHandle(_, _)).Times(AtLeast(1));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(
          Invoke([&received_frame_infos, &received_frame_count, &wait_loop](
                     int32_t buffer_id, int32_t frame_feedback_id,
                     mojom::ScopedAccessPermissionPtr* access_permission,
                     media::mojom::VideoFrameInfoPtr* frame_info) {
            if (received_frame_count >= num_frames_to_receive)
              return;
            auto& received_frame_info =
                received_frame_infos[received_frame_count];
            received_frame_info.pixel_format = (*frame_info)->pixel_format;
            received_frame_info.storage_type = (*frame_info)->storage_type;
            received_frame_info.size = (*frame_info)->coded_size;
            received_frame_info.timestamp = (*frame_info)->timestamp;
            received_frame_count += 1;
            if (received_frame_count == num_frames_to_receive)
              wait_loop.Quit();
          }));

  fake_device_proxy_->Start(requestable_settings_, std::move(receiver_proxy));

  wait_loop.Run();

  base::TimeDelta previous_timestamp;
  for (int i = 0; i < num_frames_to_receive; i++) {
    auto& frame_info = received_frame_infos[i];
    // Service is expected to always output I420
    EXPECT_EQ(media::PIXEL_FORMAT_I420, frame_info.pixel_format);
    // Service is expected to always use PIXEL_STORAGE_CPU
    EXPECT_EQ(media::PIXEL_STORAGE_CPU, frame_info.storage_type);
    EXPECT_EQ(requestable_settings_.requested_format.frame_size,
              frame_info.size);
    // Timestamps are expected to increase
    if (i > 0)
      EXPECT_GT(frame_info.timestamp, previous_timestamp);
    previous_timestamp = frame_info.timestamp;
  }
}

// Tests that buffers get reused when receiving more frames than the maximum
// number of buffers in the pool.
TEST_F(FakeVideoCaptureDeviceTest, BuffersGetReused) {
  base::RunLoop wait_loop;
  const int kMaxBufferPoolBuffers =
      DeviceMediaToMojoAdapter::max_buffer_pool_buffer_count();
  // Constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int kNumFramesToWaitFor = kMaxBufferPoolBuffers + 3;
  int num_buffers_created = 0;
  int num_frames_arrived = 0;
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, DoOnNewBufferHandle(_, _))
      .WillRepeatedly(InvokeWithoutArgs(
          [&num_buffers_created]() { num_buffers_created++; }));
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .WillRepeatedly(InvokeWithoutArgs([&wait_loop, &num_frames_arrived]() {
        if (++num_frames_arrived >= kNumFramesToWaitFor) {
          wait_loop.Quit();
        }
      }));

  fake_device_proxy_->Start(requestable_settings_, std::move(receiver_proxy));
  wait_loop.Run();

  ASSERT_LT(num_buffers_created, num_frames_arrived);
  ASSERT_LE(num_buffers_created, kMaxBufferPoolBuffers);
}

// Tests that the service requests to be closed when the last client disconnects
// while using a device.
TEST_F(FakeVideoCaptureDeviceTest,
       ServiceQuitsWhenClientDisconnectsWhileUsingDevice) {
  base::RunLoop wait_loop;
  EXPECT_CALL(*service_state_observer_, OnServiceStopped(_))
      .WillOnce(Invoke([&wait_loop](const service_manager::Identity& identity) {
        if (identity.name() == mojom::kServiceName)
          wait_loop.Quit();
      }));

  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  fake_device_proxy_->Start(requestable_settings_, std::move(receiver_proxy));

  fake_device_proxy_.reset();
  factory_.reset();
  factory_provider_.reset();

  wait_loop.Run();
}

}  // namespace video_capture
