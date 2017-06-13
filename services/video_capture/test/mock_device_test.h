// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICE_TEST_H_

#include "base/test/mock_callback.h"
#include "media/capture/video/video_capture_device.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/public/interfaces/device_factory_provider.mojom.h"
#include "services/video_capture/test/mock_device_factory.h"
#include "services/video_capture/test/mock_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class MessageLoop;
}

namespace video_capture {

// To ensure correct operation, this mock device holds on to the |client|
// that is passed to it in AllocateAndStart() and releases it on
// StopAndDeAllocate().
class MockDevice : public media::VideoCaptureDevice {
 public:
  MockDevice();
  ~MockDevice() override;

  void SendStubFrame(const media::VideoCaptureFormat& format,
                     int rotation,
                     int frame_feedback_id);

  // media::VideoCaptureDevice implementation.
  MOCK_METHOD2(DoAllocateAndStart,
               void(const media::VideoCaptureParams& params,
                    std::unique_ptr<Client>* client));
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD0(DoStopAndDeAllocate, void());
  MOCK_METHOD1(DoGetPhotoState, void(GetPhotoStateCallback* callback));
  MOCK_METHOD2(DoSetPhotoOptions,
               void(media::mojom::PhotoSettingsPtr* settings,
                    SetPhotoOptionsCallback* callback));
  MOCK_METHOD1(DoTakePhoto, void(TakePhotoCallback* callback));
  MOCK_METHOD2(OnUtilizationReport,
               void(int frame_feedback_id, double utilization));

  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

 private:
  std::unique_ptr<Client> client_;
};

// Reusable test setup for testing with a single mock device.
class MockDeviceTest : public ::testing::Test {
 public:
  MockDeviceTest();
  ~MockDeviceTest() override;

  void SetUp() override;

 protected:
  MockDeviceFactory* mock_device_factory_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> mock_device_factory_adapter_;

  mojom::DeviceFactoryPtr factory_;
  std::unique_ptr<mojo::Binding<mojom::DeviceFactory>> mock_factory_binding_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_infos_receiver_;

  MockDevice mock_device_;
  std::unique_ptr<MockReceiver> mock_receiver_;
  mojom::DevicePtr device_proxy_;
  mojom::ReceiverPtr mock_receiver_proxy_;
  media::VideoCaptureParams requested_settings_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  service_manager::ServiceContextRefFactory ref_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICE_TEST_H_
