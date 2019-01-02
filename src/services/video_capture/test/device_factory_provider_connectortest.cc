// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/service_impl.h"
#include "services/video_capture/test/mock_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

using testing::Exactly;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

// Test fixture that creates a video_capture::ServiceImpl and sets up a
// local service_manager::Connector through which client code can connect to
// it.
class DeviceFactoryProviderConnectorTest : public ::testing::Test {
 public:
  DeviceFactoryProviderConnectorTest() {}
  ~DeviceFactoryProviderConnectorTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    // We need to set the shutdown delay to at least some epsilon > 0 in order
    // to avoid the service shutting down synchronously which would prevent
    // test case ServiceIncreasesRefCountOnNewConnectionAfterDisconnect from
    // being able to reconnect before the timer expires.
    static const float kShutdownDelayInSeconds = 0.0001f;
    std::unique_ptr<ServiceImpl> service_impl =
        std::make_unique<ServiceImpl>(kShutdownDelayInSeconds);
    service_impl->SetDestructionObserver(base::BindOnce(
        [](base::RunLoop* service_destroyed_wait_loop) {
          service_destroyed_wait_loop->Quit();
        },
        &service_destroyed_wait_loop_));
    service_impl_ = service_impl.get();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::move(service_impl), true /*release_service_on_quit_request*/);
    connector_ = connector_factory_->CreateConnector();
    {
      base::RunLoop wait_loop;
      service_impl_->SetFactoryProviderClientConnectedObserver(
          wait_loop.QuitClosure());
      connector_->BindInterface(mojom::kServiceName, &factory_provider_);
      wait_loop.Run();
    }
  }

  void TearDown() override {
    if (factory_provider_.is_bound()) {
      factory_provider_.reset();
      service_destroyed_wait_loop_.Run();
    }
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ServiceImpl* service_impl_;
  mojom::DeviceFactoryProviderPtr factory_provider_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;
  std::unique_ptr<service_manager::Connector> connector_;
  base::RunLoop service_destroyed_wait_loop_;

 private:
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
};

// Tests that the service does not quit when a client connects
// while a second client stays connected.
TEST_F(DeviceFactoryProviderConnectorTest,
       ServiceDoesNotQuitWhenOneOfTwoClientsDisconnects) {
  // Establish second connection
  mojom::DeviceFactoryProviderPtr second_connection;
  {
    base::RunLoop wait_loop;
    service_impl_->SetFactoryProviderClientConnectedObserver(
        wait_loop.QuitClosure());
    connector_->BindInterface(mojom::kServiceName, &second_connection);
    wait_loop.Run();
  }

  // Release first connection
  {
    base::RunLoop wait_loop;
    service_impl_->SetFactoryProviderClientDisconnectedObserver(
        wait_loop.QuitClosure());
    factory_provider_.reset();
    wait_loop.Run();
  }

  EXPECT_FALSE(service_impl_->HasNoContextRefs());
  EXPECT_FALSE(factory_provider_.is_bound());
  EXPECT_TRUE(second_connection.is_bound());
}

// Tests that the service quits when the only client disconnects after not
// having done anything other than obtaining a connection to the fake device
// factory.
TEST_F(DeviceFactoryProviderConnectorTest,
       ServiceQuitsWhenSingleClientDisconnected) {
  mojom::DeviceFactoryPtr factory;
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));
  factory.reset();
  factory_provider_.reset();

  service_destroyed_wait_loop_.Run();
}

// Tests that the service quits when both of two clients disconnect.
TEST_F(DeviceFactoryProviderConnectorTest,
       ServiceQuitsWhenAllClientsDisconnected) {
  // Bind another client to the DeviceFactoryProvider interface.
  mojom::DeviceFactoryProviderPtr second_connection;
  connector_->BindInterface(mojom::kServiceName, &second_connection);

  mojom::DeviceFactoryPtr factory;
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));
  factory.reset();
  factory_provider_.reset();
  second_connection.reset();

  service_destroyed_wait_loop_.Run();
}

// Tests that the service increase the context ref count when a new connection
// comes in after all previous connections have been released.
TEST_F(DeviceFactoryProviderConnectorTest,
       ServiceIncreasesRefCountOnNewConnectionAfterDisconnect) {
  base::RunLoop wait_loop;
  service_impl_->SetShutdownTimeoutCancelledObserver(base::BindRepeating(
      [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
  // Disconnect and reconnect
  factory_provider_.reset();
  connector_->BindInterface(mojom::kServiceName, &factory_provider_);
  wait_loop.Run();

  EXPECT_FALSE(service_impl_->HasNoContextRefs());
}

// Tests that the service quits when the last client disconnects while using a
// device.
TEST_F(DeviceFactoryProviderConnectorTest,
       ServiceQuitsWhenClientDisconnectsWhileUsingDevice) {
  mojom::DeviceFactoryPtr factory;
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));

  // Connect to and start first device (in this case a fake camera).
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;
  media::VideoCaptureDeviceInfo fake_device_info;
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(device_info_receiver_, Run(_))
        .WillOnce(Invoke(
            [&fake_device_info, &wait_loop](
                const std::vector<media::VideoCaptureDeviceInfo>& infos) {
              fake_device_info = infos[0];
              wait_loop.Quit();
            }));
    factory->GetDeviceInfos(device_info_receiver_.Get());
    wait_loop.Run();
  }
  mojom::DevicePtr fake_device;
  factory->CreateDevice(
      std::move(fake_device_info.descriptor.device_id),
      mojo::MakeRequest(&fake_device),
      base::BindOnce([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));
  media::VideoCaptureParams requestable_settings;
  requestable_settings.requested_format = fake_device_info.supported_formats[0];
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver mock_receiver(mojo::MakeRequest(&receiver_proxy));
  fake_device->Start(requestable_settings, std::move(receiver_proxy));
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(mock_receiver, OnStarted()).WillOnce([&wait_loop]() {
      wait_loop.Quit();
    });
    wait_loop.Run();
  }

  // Disconnect
  fake_device.reset();
  factory.reset();
  factory_provider_.reset();

  service_destroyed_wait_loop_.Run();
}

}  // namespace video_capture
