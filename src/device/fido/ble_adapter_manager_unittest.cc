// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::_;

constexpr char kTestBluetoothDeviceAddress[] = "test_device_address";
constexpr char kTestFidoBleAuthenticatorId[] = "ble:test_device_address";
constexpr char kTestPinCode[] = "1234";
constexpr char kTestBluetoothDisplayName[] = "device_name";

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
  MOCK_METHOD2(FidoAuthenticatorIdChanged,
               void(base::StringPiece old_authenticator_id,
                    std::string new_authenticator_id));
  MOCK_METHOD1(FidoAuthenticatorPairingModeChanged, void(base::StringPiece));

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

  void SimulateFidoRequestHandlerHasAuthenticator(bool simulate_authenticator) {
    simulate_authenticator_ = simulate_authenticator;
  }

 private:
  void DispatchRequest(FidoAuthenticator*) override {}

  bool HasAuthenticator(
      const std::string& authentictator_address) const override {
    return simulate_authenticator_;
  }

  bool simulate_authenticator_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeFidoRequestHandlerBase);
};

}  // namespace

class FidoBleAdapterManagerTest : public ::testing::Test {
 public:
  FidoBleAdapterManagerTest() {
    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  std::unique_ptr<BleAdapterManager> CreateTestBleAdapterManager() {
    return std::make_unique<BleAdapterManager>(fake_request_handler_.get());
  }

  MockBluetoothDevice* AddMockBluetoothDeviceToAdapter() {
    auto mock_bluetooth_device = std::make_unique<MockBluetoothDevice>(
        adapter_.get(), 0 /* bluetooth_class */, kTestBluetoothDisplayName,
        kTestBluetoothDeviceAddress, false /* paired */, false /* connected */);

    auto* mock_bluetooth_device_ptr = mock_bluetooth_device.get();
    adapter_->AddMockDevice(std::move(mock_bluetooth_device));
    return mock_bluetooth_device_ptr;
  }

  MockBluetoothAdapter* adapter() { return adapter_.get(); }
  MockTransportAvailabilityObserver* observer() { return mock_observer_.get(); }
  bool adapter_powered_on_programmatically(
      const BleAdapterManager& adapter_manager) {
    return adapter_manager.adapter_powered_on_programmatically_;
  }

  FakeFidoRequestHandlerBase* fake_request_handler() {
    return fake_request_handler_.get();
  }

  const base::flat_map<std::string, std::string>& device_pincode_map(
      const FidoBlePairingDelegate& delegate) const {
    return delegate.bluetooth_device_pincode_map_;
  }

  const FidoBlePairingDelegate& ble_pairing_delegate(
      const BleAdapterManager& ble_adapter_manager) {
    return ble_adapter_manager.pairing_delegate_;
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

TEST_F(FidoBleAdapterManagerTest, AdapaterNotPresent) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(false));

  FidoRequestHandlerBase::TransportAvailabilityInfo data;
  EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
      .WillOnce(::testing::SaveArg<0>(&data));

  CreateTestBleAdapterManager();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(data.is_ble_powered);
  EXPECT_FALSE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, AdapaterPresentAndPowered) {
  EXPECT_CALL(*adapter(), IsPresent()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), IsPowered()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), CanPower()).WillOnce(::testing::Return(false));

  FidoRequestHandlerBase::TransportAvailabilityInfo data;
  EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
      .WillOnce(::testing::SaveArg<0>(&data));

  CreateTestBleAdapterManager();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_TRUE(data.is_ble_powered);
  EXPECT_FALSE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, AdapaterPresentAndCanBePowered) {
  EXPECT_CALL(*adapter(), IsPresent).WillOnce(::testing::Return(true));
  EXPECT_CALL(*adapter(), IsPowered).WillOnce(::testing::Return(false));
  EXPECT_CALL(*adapter(), CanPower).WillOnce(::testing::Return(true));

  FidoRequestHandlerBase::TransportAvailabilityInfo data;
  EXPECT_CALL(*observer(), OnTransportAvailabilityEnumerated(_))
      .WillOnce(::testing::SaveArg<0>(&data));

  CreateTestBleAdapterManager();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_FALSE(data.is_ble_powered);
  EXPECT_TRUE(data.can_power_on_ble_adapter);
}

TEST_F(FidoBleAdapterManagerTest, SetBluetoothPowerOn) {
  auto power_manager = CreateTestBleAdapterManager();
  ::testing::InSequence s;
  EXPECT_CALL(*adapter(), SetPowered(true, _, _));
  EXPECT_CALL(*adapter(), SetPowered(false, _, _));
  power_manager->SetAdapterPower(true /* set_power_on */);
  EXPECT_TRUE(adapter_powered_on_programmatically(*power_manager));
  power_manager.reset();
}

TEST_F(FidoBleAdapterManagerTest, SuccessfulPairing) {
  fake_request_handler()->SimulateFidoRequestHandlerHasAuthenticator(
      true /* simulate_authenticator */);
  auto* mock_bluetooth_device = AddMockBluetoothDeviceToAdapter();

  EXPECT_CALL(*adapter(), GetDevices())
      .WillRepeatedly(::testing::Return(adapter()->GetConstMockDevices()));
  EXPECT_CALL(*mock_bluetooth_device, Pair)
      .WillOnce(::testing::WithArgs<0, 1>(
          [mock_bluetooth_device](
              BluetoothDevice::PairingDelegate* delegate,
              const base::RepeatingClosure& success_callback) {
            delegate->RequestPinCode(mock_bluetooth_device);
            success_callback.Run();
          }));
  EXPECT_CALL(*mock_bluetooth_device, SetPinCode(kTestPinCode));

  auto adapter_manager = CreateTestBleAdapterManager();
  test::TestCallbackReceiver<> callback_receiver;
  adapter_manager->InitiatePairing(kTestFidoBleAuthenticatorId, kTestPinCode,
                                   callback_receiver.callback(),
                                   base::DoNothing());
  callback_receiver.WaitForCallback();

  const auto& pin_code_map =
      device_pincode_map(ble_pairing_delegate(*adapter_manager));
  EXPECT_EQ(1u, pin_code_map.size());
  ASSERT_TRUE(base::ContainsKey(pin_code_map, kTestFidoBleAuthenticatorId));
  EXPECT_EQ(kTestPinCode,
            pin_code_map.find(kTestFidoBleAuthenticatorId)->second);
}

TEST_F(FidoBleAdapterManagerTest, PairingFailsOnUnknownDevice) {
  auto* mock_bluetooth_device = AddMockBluetoothDeviceToAdapter();

  EXPECT_CALL(*adapter(), GetDevices())
      .WillRepeatedly(::testing::Return(adapter()->GetConstMockDevices()));
  EXPECT_CALL(*mock_bluetooth_device, Pair).Times(0);

  auto power_manager = CreateTestBleAdapterManager();
  test::TestCallbackReceiver<> callback_receiver;
  power_manager->InitiatePairing(kTestFidoBleAuthenticatorId, kTestPinCode,
                                 base::DoNothing(),
                                 callback_receiver.callback());
  callback_receiver.WaitForCallback();

  const auto& pin_code_map =
      device_pincode_map(ble_pairing_delegate(*power_manager));
  EXPECT_TRUE(pin_code_map.empty());
}

TEST_F(FidoBleAdapterManagerTest, PairingCancelledOnDestruction) {
  fake_request_handler()->SimulateFidoRequestHandlerHasAuthenticator(
      true /* simulate_authenticator */);
  auto* mock_bluetooth_device = AddMockBluetoothDeviceToAdapter();

  EXPECT_CALL(*adapter(), GetDevices())
      .WillRepeatedly(::testing::Return(adapter()->GetConstMockDevices()));
  EXPECT_CALL(*mock_bluetooth_device, Pair)
      .WillOnce(::testing::WithArg<1>(
          [](const base::RepeatingClosure& success_callback) {
            success_callback.Run();
          }));

  auto adapter_manager = CreateTestBleAdapterManager();
  test::TestCallbackReceiver<> callback_receiver;
  adapter_manager->InitiatePairing(kTestFidoBleAuthenticatorId, kTestPinCode,
                                   callback_receiver.callback(),
                                   base::DoNothing());
  callback_receiver.WaitForCallback();

  const auto& pin_code_map =
      device_pincode_map(ble_pairing_delegate(*adapter_manager));
  EXPECT_EQ(1u, pin_code_map.size());
  ASSERT_TRUE(base::ContainsKey(pin_code_map, kTestFidoBleAuthenticatorId));
  EXPECT_EQ(kTestPinCode,
            pin_code_map.find(kTestFidoBleAuthenticatorId)->second);

  // Destroying BleAdapterManager should call CancelPairing() on all
  // BluetoothDevice which has been attempted to be paried by the pairing
  // delegate.
  testing::Mock::VerifyAndClearExpectations(mock_bluetooth_device);
  EXPECT_CALL(*mock_bluetooth_device, CancelPairing);
  adapter_manager.reset();
}

}  // namespace device
