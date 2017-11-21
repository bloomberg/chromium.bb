// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "media/capture/video/video_capture_device_info.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/test/mock_producer.h"
#include "services/video_capture/test/mock_receiver.h"
#include "services/video_capture/virtual_device_mojo_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;

namespace video_capture {

namespace {

const std::string kTestDeviceId = "/test/device";
const std::string kTestDeviceName = "Test Device";
const gfx::Size kTestFrameSize = {640 /* width */, 480 /* height */};
const media::VideoPixelFormat kTestPixelFormat =
    media::VideoPixelFormat::PIXEL_FORMAT_I420;
const media::VideoPixelStorage kTestPixelStorage =
    media::VideoPixelStorage::PIXEL_STORAGE_CPU;

}  // anonymous namespace

class VirtualDeviceTest : public ::testing::Test {
 public:
  VirtualDeviceTest() : ref_factory_(base::Bind(&base::DoNothing)) {}
  ~VirtualDeviceTest() override {}

  void SetUp() override {
    device_info_.descriptor.device_id = kTestDeviceId;
    device_info_.descriptor.display_name = kTestDeviceName;
    mojom::ProducerPtr producer_proxy;
    producer_ =
        std::make_unique<MockProducer>(mojo::MakeRequest(&producer_proxy));
    device_adapter_ = std::make_unique<VirtualDeviceMojoAdapter>(
        ref_factory_.CreateRef(), device_info_, std::move(producer_proxy));
  }

  void OnFrameBufferReceived(bool valid_buffer_expected, int32_t buffer_id) {
    if (!valid_buffer_expected) {
      EXPECT_EQ(-1, buffer_id);
      return;
    }

    EXPECT_NE(-1, buffer_id);
    received_buffer_ids_.push_back(buffer_id);
  }

  void VerifyAndGetMaxFrameBuffers() {
    base::RunLoop wait_loop;
    EXPECT_CALL(*producer_, DoOnNewBufferHandle(_, _, _))
        .Times(VirtualDeviceMojoAdapter::max_buffer_pool_buffer_count())
        .WillRepeatedly(
            Invoke([](int32_t buffer_id, mojo::ScopedSharedBufferHandle* handle,
                      mojom::Producer::OnNewBufferHandleCallback& callback) {
              std::move(callback).Run();
            }));
    // Should receive valid buffer for up to the maximum buffer count.
    for (int i = 0;
         i < VirtualDeviceMojoAdapter::max_buffer_pool_buffer_count(); i++) {
      device_adapter_->RequestFrameBuffer(
          kTestFrameSize, kTestPixelFormat, kTestPixelStorage,
          base::Bind(&VirtualDeviceTest::OnFrameBufferReceived,
                     base::Unretained(this), true /* valid_buffer_expected */));
    }

    // No more buffer available. Invalid buffer id should be returned.
    device_adapter_->RequestFrameBuffer(
        kTestFrameSize, kTestPixelFormat, kTestPixelStorage,
        base::Bind(&VirtualDeviceTest::OnFrameBufferReceived,
                   base::Unretained(this), false /* valid_buffer_expected */));

    wait_loop.RunUntilIdle();
    Mock::VerifyAndClearExpectations(producer_.get());
    EXPECT_EQ(VirtualDeviceMojoAdapter::max_buffer_pool_buffer_count(),
              static_cast<int>(received_buffer_ids_.size()));
  }

 protected:
  std::unique_ptr<VirtualDeviceMojoAdapter> device_adapter_;
  // ID of buffers received and owned by the producer.
  std::vector<int> received_buffer_ids_;
  std::unique_ptr<MockProducer> producer_;

 private:
  base::MessageLoop loop_;
  service_manager::ServiceContextRefFactory ref_factory_;
  media::VideoCaptureDeviceInfo device_info_;
};

TEST_F(VirtualDeviceTest, VerifyDeviceInfo) {
  EXPECT_EQ(kTestDeviceId, device_adapter_->device_info().descriptor.device_id);
  EXPECT_EQ(kTestDeviceName,
            device_adapter_->device_info().descriptor.display_name);
}

TEST_F(VirtualDeviceTest, OnFrameReadyInBufferWithoutReceiver) {
  // Obtain maximum number of buffers.
  VerifyAndGetMaxFrameBuffers();

  base::RunLoop wait_loop;

  // Release one buffer back to the pool, no consumer hold since there is no
  // receiver.
  device_adapter_->OnFrameReadyInBuffer(received_buffer_ids_.at(0),
                                        media::mojom::VideoFrameInfo::New());

  // Verify there is a buffer available now, without creating a new
  // buffer.
  EXPECT_CALL(*producer_, DoOnNewBufferHandle(_, _, _)).Times(0);
  device_adapter_->RequestFrameBuffer(
      kTestFrameSize, kTestPixelFormat, kTestPixelStorage,
      base::Bind(&VirtualDeviceTest::OnFrameBufferReceived,
                 base::Unretained(this), true /* valid_buffer_expected */));

  wait_loop.RunUntilIdle();
}

TEST_F(VirtualDeviceTest, OnFrameReadyInBufferWithReceiver) {
  // Obtain maximum number of buffers for the producer.
  VerifyAndGetMaxFrameBuffers();

  // Release all buffers back to consumer, then back to the pool
  // after |Receiver::OnFrameReadyInBuffer| is invoked.
  base::RunLoop wait_loop;
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver receiver(mojo::MakeRequest(&receiver_proxy));
  EXPECT_CALL(receiver, OnStarted());
  EXPECT_CALL(receiver, DoOnNewBufferHandle(_, _))
      .Times(VirtualDeviceMojoAdapter::max_buffer_pool_buffer_count());
  EXPECT_CALL(receiver, DoOnFrameReadyInBuffer(_, _, _, _))
      .Times(VirtualDeviceMojoAdapter::max_buffer_pool_buffer_count());
  device_adapter_->Start(media::VideoCaptureParams(),
                         std::move(receiver_proxy));
  for (auto buffer_id : received_buffer_ids_) {
    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    // |info->metadata| cannot be a nullptr when going over mojo boundary.
    info->metadata = std::make_unique<base::DictionaryValue>();
    device_adapter_->OnFrameReadyInBuffer(buffer_id, std::move(info));
  }
  wait_loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&receiver);

  // Verify that requesting a buffer doesn't create a new one, will reuse
  // the available buffer in the pool.
  base::RunLoop wait_loop2;
  EXPECT_CALL(*producer_, DoOnNewBufferHandle(_, _, _)).Times(0);
  base::MockCallback<mojom::VirtualDevice::RequestFrameBufferCallback>
      request_frame_buffer_callback;
  EXPECT_CALL(request_frame_buffer_callback, Run(_))
      .Times(1)
      .WillOnce(Invoke([this](int32_t buffer_id) {
        // Verify that the returned |buffer_id| is a known buffer ID.
        EXPECT_TRUE(base::ContainsValue(received_buffer_ids_, buffer_id));
      }));
  device_adapter_->RequestFrameBuffer(kTestFrameSize, kTestPixelFormat,
                                      kTestPixelStorage,
                                      request_frame_buffer_callback.Get());
  wait_loop2.RunUntilIdle();
}

}  // namespace video_capture
