// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/feature_status_tracker/fake_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/mock_quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"
#include "ash/quick_pair/message_stream/fake_message_stream_lookup.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/pairing/fake_retroactive_pairing_detector.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/repository/mock_fast_pair_repository.h"
#include "ash/quick_pair/scanning/mock_scanner_broker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/mock_ui_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/fake_discovery_session_manager.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using testing::Return;

constexpr char kTestMetadataId[] = "test_metadata_id";
constexpr char kTestAddress[] = "test_address";

}  // namespace

namespace ash {
namespace quick_pair {

class MediatorTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*adapter_, IsPresent()).WillByDefault(testing::Return(true));
    ON_CALL(*adapter_, GetLowEnergyScanSessionHardwareOffloadingStatus())
        .WillByDefault(testing::Return(
            device::BluetoothAdapter::
                LowEnergyScanSessionHardwareOffloadingStatus::kSupported));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::unique_ptr<FeatureStatusTracker> tracker =
        std::make_unique<FakeFeatureStatusTracker>();
    feature_status_tracker_ =
        static_cast<FakeFeatureStatusTracker*>(tracker.get());

    std::unique_ptr<ScannerBroker> scanner_broker =
        std::make_unique<MockScannerBroker>();
    mock_scanner_broker_ =
        static_cast<MockScannerBroker*>(scanner_broker.get());

    std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector =
        std::make_unique<FakeRetroactivePairingDetector>();
    fake_retroactive_pairing_detector_ =
        static_cast<FakeRetroactivePairingDetector*>(
            retroactive_pairing_detector.get());

    std::unique_ptr<PairerBroker> pairer_broker =
        std::make_unique<MockPairerBroker>();
    mock_pairer_broker_ = static_cast<MockPairerBroker*>(pairer_broker.get());

    std::unique_ptr<UIBroker> ui_broker = std::make_unique<MockUIBroker>();
    mock_ui_broker_ = static_cast<MockUIBroker*>(ui_broker.get());

    std::unique_ptr<FastPairRepository> fast_pair_repository =
        std::make_unique<MockFastPairRepository>();
    mock_fast_pair_repository_ =
        static_cast<MockFastPairRepository*>(fast_pair_repository.get());

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(&pref_service_));
    pref_service_.registry()->RegisterBooleanPref(ash::prefs::kFastPairEnabled,
                                                  /*default_value=*/true);

    FastPairHandshakeLookup::SetCreateFunctionForTesting(base::BindRepeating(
        &MediatorTest::CreateHandshake, base::Unretained(this)));

    mediator_ = std::make_unique<Mediator>(
        std::move(tracker), std::move(scanner_broker),
        std::move(retroactive_pairing_detector),
        std::make_unique<FakeMessageStreamLookup>(), std::move(pairer_broker),
        std::move(ui_broker), std::move(fast_pair_repository),
        std::make_unique<QuickPairProcessManagerImpl>());

    device_ = base::MakeRefCounted<Device>(kTestMetadataId, kTestAddress,
                                           Protocol::kFastPairInitial);
    base::RunLoop().RunUntilIdle();
  }

  void SetHasAtLeastOneDiscoverySessionChanged(
      bool has_at_least_one_discovery_session) {
    fake_discovery_session_manager()->SetIsDiscoverySessionActive(
        /*has_at_least_one_discovery_session = */
        has_at_least_one_discovery_session);
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<FastPairHandshake> CreateHandshake(
      scoped_refptr<Device> device,
      FastPairHandshake::OnCompleteCallback callback) {
    auto fake = std::make_unique<FakeFastPairHandshake>(
        adapter_, std::move(device), std::move(callback));
    return fake;
  }

 protected:
  chromeos::bluetooth_config::FakeDiscoverySessionManager*
  fake_discovery_session_manager() {
    return ash_test_helper()
        ->bluetooth_config_test_helper()
        ->fake_discovery_session_manager();
  }

  scoped_refptr<Device> device_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  FakeFeatureStatusTracker* feature_status_tracker_;
  MockScannerBroker* mock_scanner_broker_;
  FakeRetroactivePairingDetector* fake_retroactive_pairing_detector_;
  MockPairerBroker* mock_pairer_broker_;
  MockUIBroker* mock_ui_broker_;
  MockFastPairRepository* mock_fast_pair_repository_;
  chromeos::bluetooth_config::FakeAdapterStateController
      fake_adapter_state_controller_;
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<Mediator> mediator_;
};

TEST_F(MediatorTest, TogglesScanningWhenFastPairEnabledChanges) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);

  // When one or more discovery sessions are active, don't toggle
  // scanning when fast pair enabled changes.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
}

TEST_F(MediatorTest, TogglesScanningWhenHasAtLeastOneDiscoverySessionChanges) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);

  // When fast pair is disabled, don't toggle scanning when "we have at
  // least one discovery session" changes.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
}

TEST_F(MediatorTest,
       CancelsPairingsWhenHasAtLeastOneDiscoverySessionChangesNotPairing) {
  // Start with fast pair enabled and one handshake in progress.
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                 base::DoNothing());
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // When one or more discovery sessions are active, stop scanning and dismiss
  // notifications. If we aren't actively pairing, dismiss all handshakes.
  EXPECT_CALL(*mock_pairer_broker_, IsPairing).WillOnce(Return(false));
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // When no discovery sessions are active, resume scanning.
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
}

TEST_F(MediatorTest,
       CancelsPairingsWhenHasAtLeastOneDiscoverySessionChangesIsPairing) {
  // Start with fast pair enabled and one handshake in progress.
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                 base::DoNothing());
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // When one or more discovery sessions are active, stop scanning and dismiss
  // notifications. Simulate the case where the user has already begun pairing
  // before opening Settings, or has initiated V1 device pair.
  EXPECT_CALL(*mock_pairer_broker_, IsPairing).WillOnce(Return(true));
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing).Times(0);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // When no discovery sessions are active, resume scanning.
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
}

TEST_F(MediatorTest, CancelsPairingsWhenFastPairDisabled) {
  // Start with fast pair enabled and one handshake in progress.
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                 base::DoNothing());
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // When Fast Pair becomes disabled, stop scanning, dismiss
  // notifications, and dismiss all handshakes.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // When Fast Pair becomes enabled, resume scanning.
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
}

TEST_F(MediatorTest, InvokesShowDiscoveryWhenDeviceFound) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery);
  mock_scanner_broker_->NotifyDeviceFound(device_);
}

TEST_F(MediatorTest, InvokesShowPairingOnAppropriateAction) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing);
  mock_ui_broker_->NotifyDiscoveryAction(device_,
                                         DiscoveryAction::kPairToDevice);
}

TEST_F(MediatorTest, InvokesShowPairing_V1) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  auto device = base::MakeRefCounted<Device>(kTestMetadataId, kTestAddress,
                                             Protocol::kFastPairInitial);
  device_->SetAdditionalData(Device::AdditionalDataType::kFastPairVersion, {1});
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(device_,
                                         DiscoveryAction::kPairToDevice);
}

TEST_F(MediatorTest, InvokesShowPairing_InvalidV1Data) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  auto device = base::MakeRefCounted<Device>(kTestMetadataId, kTestAddress,
                                             Protocol::kFastPairInitial);
  device_->SetAdditionalData(Device::AdditionalDataType::kFastPairVersion,
                             {1, 2});
  EXPECT_CALL(*mock_ui_broker_, ShowPairing);
  mock_ui_broker_->NotifyDiscoveryAction(device_,
                                         DiscoveryAction::kPairToDevice);
}

TEST_F(MediatorTest, DoesNotInvokeShowPairing_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(device_,
                                         DiscoveryAction::kDismissedByUser);
}

TEST_F(MediatorTest, DoesNotInvokeShowPairing_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(device_, DiscoveryAction::kDismissed);
}

TEST_F(MediatorTest, DoesNotInvokeShowPairing_LearnMore) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(device_, DiscoveryAction::kLearnMore);
}

TEST_F(MediatorTest, NotifyPairFailure_KeyBasedPairingCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeyBasedPairingCharacteristicDiscovery);
}

TEST_F(MediatorTest, NotifyPairFailure_CreateGattConnection) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kCreateGattConnection);
}

TEST_F(MediatorTest, NotifyPairFailure_GattServiceDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kGattServiceDiscovery);
}

TEST_F(MediatorTest, NotifyPairFailure_GattServiceDiscoveryTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kGattServiceDiscoveryTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_DataEncryptorRetrieval) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kDataEncryptorRetrieval);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kPasskeyCharacteristicDiscovery);
}

TEST_F(MediatorTest, NotifyPairFailure_AccountKeyCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kAccountKeyCharacteristicDiscovery);
}

TEST_F(MediatorTest,
       NotifyPairFailure_KeyBasedPairingCharacteristicNotifySession) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeyBasedPairingCharacteristicNotifySession);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyCharacteristicNotifySession) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kPasskeyCharacteristicNotifySession);
}

TEST_F(MediatorTest,
       NotifyPairFailure_KeyBasedPairingCharacteristicNotifySessionTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout);
}

TEST_F(MediatorTest,
       NotifyPairFailure_PasskeyCharacteristicNotifySessionTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kPasskeyCharacteristicNotifySessionTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_KeyBasedPairingCharacteristicWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeyBasedPairingCharacteristicWrite);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyPairingCharacteristicWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kPasskeyPairingCharacteristicWrite);
}

TEST_F(MediatorTest, NotifyPairFailure_KeyBasedPairingResponseTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyResponseTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kPasskeyResponseTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_KeybasedPairingResponseDecryptFailure) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeybasedPairingResponseDecryptFailure);
}

TEST_F(MediatorTest, NotifyPairFailure_IncorrectKeyBasedPairingResponseType) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kIncorrectKeyBasedPairingResponseType);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyDecryptFailure) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kPasskeyDecryptFailure);
}

TEST_F(MediatorTest, NotifyPairFailure_IncorrectPasskeyResponseType) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kIncorrectPasskeyResponseType);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyMismatch) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kPasskeyMismatch);
}

TEST_F(MediatorTest, NotifyPairFailure_PairingDeviceLost) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_,
                                         PairFailure::kPairingDeviceLost);
}

TEST_F(MediatorTest, NotifyPairFailure_PairingConnect) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_, PairFailure::kPairingConnect);
}

TEST_F(MediatorTest, NotifyPairFailure_AddressConnect) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(device_, PairFailure::kAddressConnect);
}

TEST_F(MediatorTest, InvokesShowAssociateAccount) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount);
  fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(device_);
}

TEST_F(MediatorTest, RemoveNotificationOnPaired) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  mock_pairer_broker_->NotifyDevicePaired(device_);
}

TEST_F(MediatorTest, RemoveNotificationOnDeviceLost) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  mock_scanner_broker_->NotifyDeviceLost(device_);
}

TEST_F(
    MediatorTest,
    NoNotificationOnAccountKeyWriteFailure_AccountKeyCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed).Times(0);
  mock_pairer_broker_->NotifyAccountKeyWrite(
      device_, AccountKeyFailure::kAccountKeyCharacteristicDiscovery);
}

TEST_F(MediatorTest,
       NoNotificationOnAccountKeyWriteFailure_kAccountKeyCharacteristicWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed).Times(0);
  mock_pairer_broker_->NotifyAccountKeyWrite(
      device_, AccountKeyFailure::kAccountKeyCharacteristicDiscovery);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_AssociateAccount) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  mock_ui_broker_->NotifyAssociateAccountAction(
      device_, AssociateAccountAction::kAssoicateAccount);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_LearnMore) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications).Times(0);
  mock_ui_broker_->NotifyAssociateAccountAction(
      device_, AssociateAccountAction::kLearnMore);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyAssociateAccountAction(
      device_, AssociateAccountAction::kDismissedByUser);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyAssociateAccountAction(
      device_, AssociateAccountAction::kDismissed);
}

TEST_F(MediatorTest, CompanionAppAction_DownloadApp) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyCompanionAppAction(
      device_, CompanionAppAction::kDownloadAndLaunchApp);
}

TEST_F(MediatorTest, CompanionAppAction_LaunchApp) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyCompanionAppAction(device_,
                                            CompanionAppAction::kLaunchApp);
}

TEST_F(MediatorTest, CompanionAppAction_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyCompanionAppAction(
      device_, CompanionAppAction::kDismissedByUser);
}

TEST_F(MediatorTest, CompanionAppAction_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyCompanionAppAction(device_,
                                            CompanionAppAction::kDismissed);
}

TEST_F(MediatorTest, PairingFailedAction_NavigateToSettings) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyPairingFailedAction(
      device_, PairingFailedAction::kNavigateToSettings);
}

TEST_F(MediatorTest, PairingFailedAction_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyPairingFailedAction(
      device_, PairingFailedAction::kDismissedByUser);
}

TEST_F(MediatorTest, PairingFailedAction_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyPairingFailedAction(device_,
                                             PairingFailedAction::kDismissed);
}

TEST_F(MediatorTest, FastPairBluetoothConfigDelegate) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  chromeos::bluetooth_config::FastPairDelegate* delegate =
      mediator_->GetFastPairDelegate();
  delegate->SetDeviceNameManager(nullptr);
  delegate->SetAdapterStateController(nullptr);
  EXPECT_TRUE(delegate);
  EXPECT_EQ(delegate->GetDeviceImageInfo(kTestMetadataId), absl::nullopt);
}

TEST_F(MediatorTest,
       FastPairBluetoothConfigDelegateNotifiesAdapterStateChanges) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                 base::DoNothing());

  chromeos::bluetooth_config::FastPairDelegate* delegate =
      mediator_->GetFastPairDelegate();
  delegate->SetDeviceNameManager(nullptr);

  // Mediator should not observe changes to the adapter state before the
  // AdapterStateController is set and the observation is created.
  fake_adapter_state_controller_.SetSystemState(
      chromeos::bluetooth_config::mojom::BluetoothSystemState::kDisabling);
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));
  fake_adapter_state_controller_.SetSystemState(
      chromeos::bluetooth_config::mojom::BluetoothSystemState::kEnabled);
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  // After the AdapterStateController is set, we should be notified and
  // create an observation.
  delegate->SetAdapterStateController(&fake_adapter_state_controller_);

  // Simulate a call to toggling Bluetooth off via the UI. This should stop
  // scanning, clear existing handshakes, stop pairing, and dismiss
  // notifications.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  fake_adapter_state_controller_.SetSystemState(
      chromeos::bluetooth_config::mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(device_));

  delegate->SetAdapterStateController(nullptr);
}

}  // namespace quick_pair
}  // namespace ash
