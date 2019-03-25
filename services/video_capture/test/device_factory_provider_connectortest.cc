// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/timer.h"
#include "media/base/media_switches.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "services/video_capture/public/cpp/mock_receiver.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_capture/service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

using testing::Exactly;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

// Test fixture that creates a video_capture::ServiceImpl and sets up a
// local service_manager::Connector through which client code can connect to
// it.
template <class DeviceFactoryProviderConnectorTestTraits>
class DeviceFactoryProviderConnectorTest : public ::testing::Test {
 public:
  DeviceFactoryProviderConnectorTest() {}
  ~DeviceFactoryProviderConnectorTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    service_impl_ = std::make_unique<ServiceImpl>(
        connector_factory_.RegisterInstance(video_capture::mojom::kServiceName),
        scoped_task_environment_.GetMainThreadTaskRunner(),
        DeviceFactoryProviderConnectorTestTraits::shutdown_delay());
    service_impl_->set_termination_closure(
        base::BindOnce(&DeviceFactoryProviderConnectorTest::OnServiceQuit,
                       base::Unretained(this)));

    connector_ = connector_factory_.CreateConnector();

    base::RunLoop wait_loop;
    service_impl_->SetFactoryProviderClientConnectedObserver(
        wait_loop.QuitClosure());
    connector_->BindInterface(mojom::kServiceName, &factory_provider_);
    wait_loop.Run();
  }

  void TearDown() override {
    if (factory_provider_.is_bound() &&
        DeviceFactoryProviderConnectorTestTraits::shutdown_delay()
            .has_value()) {
      factory_provider_.reset();
      service_destroyed_wait_loop_.Run();
    }
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<ServiceImpl> service_impl_;
  mojom::DeviceFactoryProviderPtr factory_provider_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;
  std::unique_ptr<service_manager::Connector> connector_;
  base::RunLoop service_destroyed_wait_loop_;
  base::OnceClosure service_quit_callback_;

 private:
  void OnServiceQuit() {
    service_impl_.reset();
    service_destroyed_wait_loop_.Quit();
    if (service_quit_callback_)
      std::move(service_quit_callback_).Run();
  }

  service_manager::TestConnectorFactory connector_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceFactoryProviderConnectorTest);
};

// We need to set the shutdown delay to at least some epsilon > 0 in order
// to avoid the service shutting down synchronously which would prevent
// test case ServiceIncreasesRefCountOnNewConnectionAfterDisconnect from
// being able to reconnect before the timer expires.
struct ShortShutdownDelayDeviceFactoryProviderConnectorTestTraits {
  static base::Optional<base::TimeDelta> shutdown_delay() {
    return base::TimeDelta::FromMicroseconds(100);
  }
};
using ShortShutdownDelayDeviceFactoryProviderConnectorTest =
    DeviceFactoryProviderConnectorTest<
        ShortShutdownDelayDeviceFactoryProviderConnectorTestTraits>;

// Tests that the service does not quit when a client connects
// while a second client stays connected.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
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
// having done anything other than obtaining a connection to the device factory.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       ServiceQuitsWhenSingleDeviceFactoryClientDisconnected) {
  mojom::DeviceFactoryPtr factory;
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));
  factory.reset();
  factory_provider_.reset();

  service_destroyed_wait_loop_.Run();
}

// Tests that the service quits when the only client disconnects after not
// having done anything other than obtaining a connection to the video source
// provider.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       ServiceQuitsWhenSingleVideoSourceProviderClientDisconnected) {
  mojom::VideoSourceProviderPtr source_provider;
  factory_provider_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));
  source_provider.reset();
  factory_provider_.reset();

  service_destroyed_wait_loop_.Run();
}

// Tests that the service quits when the only client disconnects after
// enumerating devices via the video source provider.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       ServiceQuitsAfterEnumeratingDevices) {
  mojom::VideoSourceProviderPtr source_provider;
  factory_provider_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .WillOnce(
          Invoke([&wait_loop](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            wait_loop.Quit();
          }));
  source_provider->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();

  source_provider.reset();
  factory_provider_.reset();

  service_destroyed_wait_loop_.Run();
}

// Tests that enumerating devices works after the only client disconnects and
// reconnects via the video source provider.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       EnumerateDevicesAfterReconnect) {
  // Connect |source_provider|.
  mojom::VideoSourceProviderPtr source_provider;
  factory_provider_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  // Disconnect |source_provider| and wait for the disconnect to propagate to
  // the service.
  {
    base::RunLoop wait_loop;
    source_provider->Close(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
    source_provider.reset();
  }

  // Reconnect |source_provider|.
  factory_provider_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  // Enumerate devices.
  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .WillOnce(
          Invoke([&wait_loop](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            wait_loop.Quit();
          }));
  source_provider->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();

  source_provider.reset();
  factory_provider_.reset();

  service_destroyed_wait_loop_.Run();
}

// Tests that the service quits when both of two clients disconnect.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
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
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
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
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       ServiceQuitsWhenClientDisconnectsWhileUsingDevice) {
  mojom::DeviceFactoryPtr factory;
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));

  // Connect to and start first device (in this case a fake camera).
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

// Tests that the service does not quit when the only client discards the
// DeviceFactoryProvider but holds on to a DeviceFactory.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       DeviceFactoryCanStillBeUsedAfterReleaseingDeviceFactoryProvider) {
  mojom::DeviceFactoryPtr factory;
  factory_provider_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));

  // Exercise: Disconnect DeviceFactoryProvider
  {
    base::RunLoop wait_loop;
    service_impl_->SetFactoryProviderClientDisconnectedObserver(
        wait_loop.QuitClosure());
    factory_provider_.reset();
    wait_loop.Run();
  }

  EXPECT_FALSE(service_impl_->HasNoContextRefs());

  // Verify that |factory| is still functional by calling GetDeviceInfos().
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(device_info_receiver_, Run(_))
        .WillOnce(Invoke(
            [&wait_loop](
                const std::vector<media::VideoCaptureDeviceInfo>& infos) {
              wait_loop.Quit();
            }));
    factory->GetDeviceInfos(device_info_receiver_.Get());
    wait_loop.Run();
  }
}

// Tests that the service does not quit when the only client discards the
// DeviceFactoryProvider but holds on to a VideoSourceProvider.
TEST_F(ShortShutdownDelayDeviceFactoryProviderConnectorTest,
       VideoSourceProviderCanStillBeUsedAfterReleaseingDeviceFactoryProvider) {
  mojom::VideoSourceProviderPtr source_provider;
  factory_provider_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  // Exercise: Disconnect DeviceFactoryProvider
  {
    base::RunLoop wait_loop;
    service_impl_->SetFactoryProviderClientDisconnectedObserver(
        wait_loop.QuitClosure());
    factory_provider_.reset();
    wait_loop.Run();
  }

  EXPECT_FALSE(service_impl_->HasNoContextRefs());

  // Verify that |source_provider| is still functional by calling
  // GetDeviceInfos().
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(device_info_receiver_, Run(_))
        .WillOnce(Invoke(
            [&wait_loop](
                const std::vector<media::VideoCaptureDeviceInfo>& infos) {
              wait_loop.Quit();
            }));
    source_provider->GetSourceInfos(device_info_receiver_.Get());
    wait_loop.Run();
  }
}

struct NoAutomaticShutdownDeviceFactoryProviderConnectorTestTraits {
  static base::Optional<base::TimeDelta> shutdown_delay() {
    return base::Optional<base::TimeDelta>();
  }
};
using NoAutomaticShutdownDeviceFactoryProviderConnectorTest =
    DeviceFactoryProviderConnectorTest<
        NoAutomaticShutdownDeviceFactoryProviderConnectorTestTraits>;

// Tests that the service does not shut down after disconnecting.
TEST_F(NoAutomaticShutdownDeviceFactoryProviderConnectorTest,
       ServiceDoesNotShutDownOnDisconnect) {
  {
    base::RunLoop wait_loop;
    service_impl_->SetFactoryProviderClientDisconnectedObserver(
        wait_loop.QuitClosure());
    factory_provider_.reset();
    wait_loop.Run();
  }

  base::MockCallback<base::OnceClosure> service_impl_destructor_cb;
  service_quit_callback_ = service_impl_destructor_cb.Get();
  EXPECT_CALL(service_impl_destructor_cb, Run()).Times(0);

  // Wait for an arbitrary short extra time after which we are convinced that
  // there is enough evidence that service is not going to shut down.
  {
    base::RunLoop wait_loop;
    base::OneShotTimer wait_timer;
    wait_timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10),
                     wait_loop.QuitClosure());
    wait_loop.Run();
  }

  service_quit_callback_.Reset();
}

}  // namespace video_capture
