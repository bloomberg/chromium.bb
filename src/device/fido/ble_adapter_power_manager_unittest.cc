// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_power_manager.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/fido_request_handler_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::_;

class MockTransportAvailabilityObserver
    : public FidoRequestHandlerBase::TransportAvailabilityObserver {
 public:
  MockTransportAvailabilityObserver() = default;
  ~MockTransportAvailabilityObserver() override = default;

  MOCK_METHOD1(OnTransportAvailabilityEnumerated,
               void(FidoRequestHandlerBase::TransportAvailabilityInfo data));
  MOCK_METHOD1(EmbedderControlsAuthenticatorDispatch,
               bool(const FidoAuthenticator& authenticator));
  MOCK_METHOD1(BluetoothAdapterPowerChanged, void(bool is_powered_on));
  MOCK_METHOD1(FidoAuthenticatorAdded,
               void(const FidoAuthenticator& authenticator));
  MOCK_METHOD1(FidoAuthenticatorRemoved, void(base::StringPiece device_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTransportAvailabilityObserver);
};

class FakeFidoRequestHandlerBase : public FidoRequestHandlerBase {
 public:
  explicit FakeFidoRequestHandlerBase(
      MockTransportAvailabilityObserver* observer)
      : FidoRequestHandlerBase(nullptr, {}) {
    set_observer(observer);
  }

 private:
  void DispatchRequest(FidoAuthenticator*) override {}

  DISALLOW_COPY_AND_ASSIGN(FakeFidoRequestHandlerBase);
};

}  // namespace

class FidoBleAdapterPowerManagerTest : public ::testing::Test {
 public:
  FidoBleAdapterPowerManagerTest() {
    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  std::unique_ptr<BleAdapterPowerManager> CreateTestBleAdapterPowerManager() {
    return std::make_unique<BleAdapterPowerManager>(
        fake_request_handler_.get());
  }

  MockBluetoothAdapter* adapter() { return adapter_.get(); }
  MockTransportAvailabilityObserver* observer() { return mock_observer_.get(); }
  bool adapter_powered_on_programmatically(
      const BleAdapterPowerManager& adapter_power_manager) {
    return adapter_power_manager.adapter_powered_on_programmatically_;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  std::unique_ptr<MockTransportAvailabilityObserver> mock_observer_ =
      std::make_unique<MockTransportAvailabilityObserver>();
  std::unique_ptr<FakeFidoRequestHandlerBase> fake_request_handler_ =
      std::make_unique<FakeFidoRequestHandlerBase>(mock_observer_.get());
};

TEST_F(FidoBleAdapterPowerManagerTest, AdapaterNotPresent) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(false));

  FidoRequestHandlerBase::TransportAvailabilityInfo data;
  EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
      .WillOnce(::testing::SaveArg<0>(&data));

  CreateTestBleAdapterPowerManager();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(data.is_ble_powered);
  EXPECT_FALSE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterPowerManagerTest, AdapaterPresentAndPowered) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(false));

  FidoRequestHandlerBase::TransportAvailabilityInfo data;
  EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
      .WillOnce(::testing::SaveArg<0>(&data));

  CreateTestBleAdapterPowerManager();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(data.is_ble_powered);
  EXPECT_FALSE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterPowerManagerTest, AdapaterPresentAndCanBePowered) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(true));

  FidoRequestHandlerBase::TransportAvailabilityInfo data;
  EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
      .WillOnce(::testing::SaveArg<0>(&data));

  CreateTestBleAdapterPowerManager();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(data.is_ble_powered);
  EXPECT_TRUE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterPowerManagerTest, TestSetBluetoothPowerOn) {
  auto power_manager = CreateTestBleAdapterPowerManager();
  ::testing::InSequence s;
  EXPECT_CALL(*adapter(), SetPowered(true, _, _));
  EXPECT_CALL(*adapter(), SetPowered(false, _, _));
  power_manager->SetAdapterPower(true /* set_power_on */);
  EXPECT_TRUE(adapter_powered_on_programmatically(*power_manager));
  power_manager.reset();
}

}  // namespace device
