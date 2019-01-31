// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_TEST_DEVICE_FACTORY_PROVIDER_TEST_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_TEST_DEVICE_FACTORY_PROVIDER_TEST_H_

#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

class MockProducer;

// Basic test fixture that sets up a connection to the fake device factory.
class DeviceFactoryProviderTest : public testing::Test {
 public:
  DeviceFactoryProviderTest();
  ~DeviceFactoryProviderTest() override;

  void SetUp() override;

 protected:
  struct SharedMemoryVirtualDeviceContext {
    SharedMemoryVirtualDeviceContext(mojom::ProducerRequest producer_request);
    ~SharedMemoryVirtualDeviceContext();

    std::unique_ptr<MockProducer> mock_producer;
    mojom::SharedMemoryVirtualDevicePtr device;
  };

  std::unique_ptr<SharedMemoryVirtualDeviceContext>
  AddSharedMemoryVirtualDevice(const std::string& device_id);

  mojom::TextureVirtualDevicePtr AddTextureVirtualDevice(
      const std::string& device_id);

  service_manager::Connector* connector() {
    return test_service_binding_.GetConnector();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::Service test_service_;
  service_manager::ServiceBinding test_service_binding_;

  mojom::DeviceFactoryProviderPtr factory_provider_;
  mojom::DeviceFactoryPtr factory_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;

  DISALLOW_COPY_AND_ASSIGN(DeviceFactoryProviderTest);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_TEST_DEVICE_FACTORY_PROVIDER_TEST_H_
