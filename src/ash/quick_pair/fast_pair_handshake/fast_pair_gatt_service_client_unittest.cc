// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"

#include <stddef.h>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

namespace {

using NotifySessionCallback = base::OnceCallback<void(
    std::unique_ptr<device::BluetoothGattNotifySession>)>;
using ErrorCallback =
    base::OnceCallback<void(device::BluetoothGattService::GattErrorCode)>;

const char kTotalGattConnectionTime[] =
    "Bluetooth.ChromeOS.FastPair.TotalGattConnectionTime";
const char kGattConnectionResult[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.Result";
const char kGattConnectionErrorMetric[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.ErrorReason";
const char kWriteKeyBasedCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.GattErrorReason";
const char kNotifyKeyBasedCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.NotifyTime";
const char kWritePasskeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.GattErrorReason";
const char kNotifyPasskeyCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.NotifyTime";
const char kWriteAccountKeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.GattErrorReason";
const char kWriteAccountKeyTimeMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.TotalTime";

constexpr base::TimeDelta kConnectingTestTimeout = base::Seconds(5);

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";
const char kTestServiceId[] = "service_id1";
const std::string kUUIDString1 = "keybased";
const std::string kUUIDString2 = "passkey";
const std::string kUUIDString3 = "accountkey";
const device::BluetoothUUID kNonFastPairUuid("0xFE2B");

const device::BluetoothUUID kKeyBasedCharacteristicUuid1("1234");
const device::BluetoothUUID kKeyBasedCharacteristicUuid2(
    "FE2C1234-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kPasskeyCharacteristicUuid1("1235");
const device::BluetoothUUID kPasskeyCharacteristicUuid2(
    "FE2C1235-8366-4814-8EB0-01DE32100BEA");
const device::BluetoothUUID kAccountKeyCharacteristicUuid1("1236");
const device::BluetoothUUID kAccountKeyCharacteristicUuid2(
    "FE2C1236-8366-4814-8EB0-01DE32100BEA");

const uint8_t kMessageType = 0x00;
const uint8_t kFlags = 0x00;
const std::string kProviderAddress = "abcde";
const std::string kSeekersAddress = "abcde";
const std::vector<uint8_t>& kTestWriteResponse{0x01, 0x03, 0x02, 0x01, 0x02};
const uint8_t kSeekerPasskey = 0x02;
const uint32_t kPasskey = 13;
const std::array<uint8_t, 16> kAccountKey = {0x04, 0x01, 0x01, 0x01, 0x01, 0x01,
                                             0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                             0x01, 0x01, 0x01, 0x01};

const std::array<uint8_t, 64> kPublicKey = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F,
    0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
    0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01,
    0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45,
    0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32,
    0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

const device::BluetoothRemoteGattCharacteristic::Properties kProperties =
    device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
    device::BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE |
    device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

const device::BluetoothRemoteGattCharacteristic::Permissions kPermissions =
    device::BluetoothRemoteGattCharacteristic::PERMISSION_READ_ENCRYPTED |
    device::BluetoothRemoteGattCharacteristic::PERMISSION_WRITE_ENCRYPTED;

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void NotifyGattDiscoveryCompleteForService(
      device::BluetoothRemoteGattService* service) {
    device::BluetoothAdapter::NotifyGattDiscoveryComplete(service);
  }

  void NotifyGattCharacteristicValueChanged(
      device::BluetoothRemoteGattCharacteristic* characteristic) {
    device::BluetoothAdapter::NotifyGattCharacteristicValueChanged(
        characteristic, kTestWriteResponse);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(FakeBluetoothAdapter* adapter, const std::string& address)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       address,
                                                       /*paired=*/true,
                                                       /*connected=*/true),
        fake_adapter_(adapter) {}

  void CreateGattConnection(
      device::BluetoothDevice::GattConnectionCallback callback,
      absl::optional<device::BluetoothUUID> service_uuid =
          absl::nullopt) override {
    gatt_connection_ = std::make_unique<
        testing::NiceMock<device::MockBluetoothGattConnection>>(
        fake_adapter_, kTestBleDeviceAddress);

    if (has_gatt_connection_error_) {
      std::move(callback).Run(std::move(gatt_connection_),
                              /*error_code=*/device::BluetoothDevice::
                                  ConnectErrorCode::ERROR_FAILED);
    } else {
      ON_CALL(*gatt_connection_.get(), IsConnected)
          .WillByDefault(testing::Return(true));
      std::move(callback).Run(std::move(gatt_connection_), absl::nullopt);
    }
  }

  void SetError(bool has_gatt_connection_error) {
    has_gatt_connection_error_ = has_gatt_connection_error;
  }

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

 protected:
  std::unique_ptr<testing::NiceMock<device::MockBluetoothGattConnection>>
      gatt_connection_;
  bool has_gatt_connection_error_ = false;
  FakeBluetoothAdapter* fake_adapter_;
};

class FakeBluetoothGattCharacteristic
    : public testing::NiceMock<device::MockBluetoothGattCharacteristic> {
 public:
  FakeBluetoothGattCharacteristic(device::MockBluetoothGattService* service,
                                  const std::string& identifier,
                                  const device::BluetoothUUID& uuid,
                                  Properties properties,
                                  Permissions permissions)
      : testing::NiceMock<device::MockBluetoothGattCharacteristic>(
            service,
            identifier,
            uuid,
            properties,
            permissions) {}

  // Move-only class
  FakeBluetoothGattCharacteristic(const FakeBluetoothGattCharacteristic&) =
      delete;
  FakeBluetoothGattCharacteristic operator=(
      const FakeBluetoothGattCharacteristic&) = delete;

  void StartNotifySession(NotifySessionCallback callback,
                          ErrorCallback error_callback) override {
    if (notify_session_error_) {
      std::move(error_callback)
          .Run(device::BluetoothGattService::GATT_ERROR_NOT_PERMITTED);
      return;
    }

    auto fake_notify_session = std::make_unique<
        testing::NiceMock<device::MockBluetoothGattNotifySession>>(
        GetWeakPtr());

    if (notify_timeout_)
      task_environment_->FastForwardBy(kConnectingTestTimeout);

    std::move(callback).Run(std::move(fake_notify_session));
  }

  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 WriteType write_type,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override {
    if (write_remote_error_) {
      std::move(error_callback)
          .Run(device::BluetoothGattService::GATT_ERROR_NOT_PERMITTED);
      return;
    }

    if (write_timeout_)
      task_environment_->FastForwardBy(kConnectingTestTimeout);

    std::move(callback).Run();
  }

  void SetWriteError(bool write_remote_error) {
    write_remote_error_ = write_remote_error;
  }

  void SetWriteTimeout(bool write_timeout,
                       base::test::TaskEnvironment* task_environment) {
    write_timeout_ = write_timeout;
    task_environment_ = task_environment;
  }

  void SetNotifySessionError(bool notify_session_error) {
    notify_session_error_ = notify_session_error;
  }

  void SetNotifySessionTimeout(bool timeout,
                               base::test::TaskEnvironment* task_environment) {
    notify_timeout_ = timeout;
    task_environment_ = task_environment;
  }

 private:
  bool notify_session_error_ = false;
  bool write_remote_error_ = false;
  bool notify_timeout_ = false;
  bool write_timeout_ = false;
  base::test::TaskEnvironment* task_environment_ = nullptr;
};

std::unique_ptr<FakeBluetoothDevice> CreateTestBluetoothDevice(
    FakeBluetoothAdapter* adapter,
    device::BluetoothUUID uuid) {
  auto mock_device = std::make_unique<FakeBluetoothDevice>(
      /*adapter=*/adapter, kTestBleDeviceAddress);
  mock_device->AddUUID(uuid);
  mock_device->SetServiceDataForUUID(uuid, {1, 2, 3});
  return mock_device;
}

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairGattServiceClientTest : public testing::Test {
 public:
  void SetUp() override { fast_pair_data_encryptor_->public_key(kPublicKey); }

  void SuccessfulGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    adapter_->AddMockDevice(std::move(device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void FailedGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    device_->SetError(true);
    adapter_->AddMockDevice(std::move(device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void NonFastPairServiceDataSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device_ = CreateTestBluetoothDevice(adapter_.get(), kNonFastPairUuid);
    adapter_->AddMockDevice(std::move(device_));
    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                InitializedTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void NotifyGattDiscoveryCompleteForService() {
    auto gatt_service =
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            CreateTestBluetoothDevice(adapter_.get(),
                                      ash::quick_pair::kFastPairBluetoothUuid)
                .get(),
            kTestServiceId, ash::quick_pair::kFastPairBluetoothUuid,
            /*is_primary=*/true);
    gatt_service_ = std::move(gatt_service);
    ON_CALL(*(gatt_service_.get()), GetDevice)
        .WillByDefault(
            testing::Return(adapter_->GetDevice(kTestBleDeviceAddress)));
    if (!keybased_char_error_) {
      fake_key_based_characteristic_ =
          std::make_unique<FakeBluetoothGattCharacteristic>(
              gatt_service_.get(), kUUIDString1, kKeyBasedCharacteristicUuid1,
              kProperties, kPermissions);

      if (keybased_notify_session_error_)
        fake_key_based_characteristic_->SetNotifySessionError(true);

      if (keybased_notify_session_timeout_) {
        fake_key_based_characteristic_->SetNotifySessionTimeout(
            true, &task_environment_);
      }

      if (key_based_write_error_) {
        fake_key_based_characteristic_->SetWriteError(true);
      }

      if (key_based_write_timeout_) {
        fake_key_based_characteristic_->SetWriteTimeout(true,
                                                        &task_environment_);
      }

      temp_fake_key_based_characteristic_ =
          fake_key_based_characteristic_.get();
      gatt_service_->AddMockCharacteristic(
          std::move(fake_key_based_characteristic_));
    }

    if (!passkey_char_error_) {
      fake_passkey_characteristic_ =
          std::make_unique<FakeBluetoothGattCharacteristic>(
              gatt_service_.get(), kUUIDString2, kPasskeyCharacteristicUuid1,
              kProperties, kPermissions);

      if (passkey_notify_session_error_)
        fake_passkey_characteristic_->SetNotifySessionError(true);

      if (passkey_notify_session_timeout_) {
        fake_passkey_characteristic_->SetNotifySessionTimeout(
            true, &task_environment_);
      }

      if (passkey_write_error_) {
        fake_passkey_characteristic_->SetWriteError(true);
      }

      if (passkey_write_timeout_) {
        fake_passkey_characteristic_->SetWriteTimeout(true, &task_environment_);
      }

      temp_passkey_based_characteristic_ = fake_passkey_characteristic_.get();

      gatt_service_->AddMockCharacteristic(
          std::move(fake_passkey_characteristic_));
    }

    auto fake_account_key_characteristic =
        std::make_unique<FakeBluetoothGattCharacteristic>(
            gatt_service_.get(), kUUIDString3, kAccountKeyCharacteristicUuid1,
            kProperties, kPermissions);
    if (account_key_write_error_) {
      fake_account_key_characteristic->SetWriteError(true);
    }

    gatt_service_->AddMockCharacteristic(
        std::move(fake_account_key_characteristic));
    adapter_->NotifyGattDiscoveryCompleteForService(gatt_service_.get());
  }

  bool ServiceIsSet() {
    if (!gatt_service_client_->gatt_service())
      return false;
    return gatt_service_client_->gatt_service() == gatt_service_.get();
  }

  void SetPasskeyCharacteristicError(bool passkey_char_error) {
    passkey_char_error_ = passkey_char_error;
  }

  void SetKeybasedCharacteristicError(bool keybased_char_error) {
    keybased_char_error_ = keybased_char_error;
  }

  void SetAccountKeyCharacteristicWriteError(bool account_key_write_error) {
    account_key_write_error_ = account_key_write_error;
  }

  void SetPasskeyNotifySessionError(bool passkey_notify_session_error) {
    passkey_notify_session_error_ = passkey_notify_session_error;
  }

  void SetKeybasedNotifySessionError(bool keybased_notify_session_error) {
    keybased_notify_session_error_ = keybased_notify_session_error;
  }

  void InitializedTestCallback(absl::optional<PairFailure> failure) {
    initalized_failure_ = failure;
  }

  absl::optional<PairFailure> GetInitializedCallbackResult() {
    return initalized_failure_;
  }

  void WriteTestCallback(std::vector<uint8_t> response,
                         absl::optional<PairFailure> failure) {
    write_failure_ = failure;
  }

  void AccountKeyCallback(
      absl::optional<device::BluetoothGattService::GattErrorCode> error) {
    gatt_error_ = error;
  }

  absl::optional<device::BluetoothGattService::GattErrorCode>
  GetAccountKeyCallback() {
    return gatt_error_;
  }

  absl::optional<PairFailure> GetWriteCallbackResult() {
    return write_failure_;
  }

  void SetPasskeyNotifySessionTimeout(bool timeout) {
    passkey_notify_session_timeout_ = timeout;
  }

  void SetKeybasedNotifySessionTimeout(bool timeout) {
    keybased_notify_session_timeout_ = timeout;
  }

  void FastForwardTimeByConnetingTimeout() {
    task_environment_.FastForwardBy(kConnectingTestTimeout);
  }

  void WriteRequestToKeyBased() {
    gatt_service_client_->WriteRequestAsync(
        kMessageType, kFlags, kProviderAddress, kSeekersAddress,
        fast_pair_data_encryptor_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                WriteTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void WriteRequestToPasskey() {
    gatt_service_client_->WritePasskeyAsync(
        kSeekerPasskey, kPasskey, fast_pair_data_encryptor_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                WriteTestCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void WriteAccountKey() {
    gatt_service_client_->WriteAccountKey(
        kAccountKey, fast_pair_data_encryptor_.get(),
        base::BindRepeating(&::ash::quick_pair::FastPairGattServiceClientTest::
                                AccountKeyCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void TriggerKeyBasedGattChanged() {
    adapter_->NotifyGattCharacteristicValueChanged(
        temp_fake_key_based_characteristic_);
  }

  void TriggerPasskeyGattChanged() {
    adapter_->NotifyGattCharacteristicValueChanged(
        temp_passkey_based_characteristic_);
  }

  void SetKeyBasedWriteError() { key_based_write_error_ = true; }

  void SetPasskeyWriteError() { passkey_write_error_ = true; }

  void SetWriteRequestTimeout() { key_based_write_timeout_ = true; }

  void SetWritePasskeyTimeout() { passkey_write_timeout_ = true; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;

 private:
  // We need temporary pointers to use for write/ready requests because we
  // move the unique pointers when we notify the session.
  FakeBluetoothGattCharacteristic* temp_fake_key_based_characteristic_;
  FakeBluetoothGattCharacteristic* temp_passkey_based_characteristic_;
  absl::optional<device::BluetoothGattService::GattErrorCode> gatt_error_ =
      absl::nullopt;
  bool passkey_char_error_ = false;
  bool keybased_char_error_ = false;
  bool account_key_write_error_ = false;
  bool passkey_notify_session_error_ = false;
  bool keybased_notify_session_error_ = false;
  bool passkey_notify_session_timeout_ = false;
  bool keybased_notify_session_timeout_ = false;
  bool key_based_write_error_ = false;
  bool key_based_write_timeout_ = false;
  bool passkey_write_error_ = false;
  bool passkey_write_timeout_ = false;

  absl::optional<PairFailure> initalized_failure_;
  absl::optional<PairFailure> write_failure_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<FakeBluetoothDevice> device_;
  std::unique_ptr<FakeBluetoothGattCharacteristic>
      fake_key_based_characteristic_;
  std::unique_ptr<FakeFastPairDataEncryptor> fast_pair_data_encryptor_ =
      std::make_unique<FakeFastPairDataEncryptor>();
  std::unique_ptr<FakeBluetoothGattCharacteristic> fake_passkey_characteristic_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
      gatt_service_;
  base::WeakPtrFactory<FastPairGattServiceClientTest> weak_ptr_factory_{this};
};

TEST_F(FastPairGattServiceClientTest, GattServiceDiscoveryTimeout) {
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  FastForwardTimeByConnetingTimeout();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kGattServiceDiscoveryTimeout);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 1);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, FailedGattConnection) {
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  FailedGattConnectionSetUp();
  EXPECT_EQ(GetInitializedCallbackResult(), PairFailure::kCreateGattConnection);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 1);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, GattConnectionSuccess) {
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 0);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_TRUE(gatt_service_client_->IsConnected());
  histogram_tester().ExpectTotalCount(kTotalGattConnectionTime, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionResult, 1);
  histogram_tester().ExpectTotalCount(kGattConnectionErrorMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, IgnoreNonFastPairServices) {
  NonFastPairServiceDataSetUp();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, FailedKeyBasedCharacteristics) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  SetKeybasedCharacteristicError(true);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicDiscovery);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, FailedPasskeyCharacteristics) {
  SetPasskeyCharacteristicError(true);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kPasskeyCharacteristicDiscovery);
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, SuccessfulCharacteristicsStartNotify) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  SetKeybasedCharacteristicError(false);
  SetPasskeyCharacteristicError(false);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, StartNotifyPasskeyFailure) {
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  SetPasskeyNotifySessionError(true);
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kPasskeyCharacteristicNotifySession);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, StartNotifyKeybasedFailure) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  SetKeybasedNotifySessionError(true);
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicNotifySession);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, PasskeyStartNotifyTimeout) {
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SetPasskeyNotifySessionTimeout(true);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kPasskeyCharacteristicNotifySessionTimeout);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, KeyBasedStartNotifyTimeout) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  SetKeybasedNotifySessionTimeout(true);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, MultipleNotifyTimeout) {
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  SetKeybasedNotifySessionTimeout(true);
  SetPasskeyNotifySessionTimeout(true);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout);
  EXPECT_FALSE(ServiceIsSet());
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
}

TEST_F(FastPairGattServiceClientTest, WriteKeyBasedRequest) {
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyKeyBasedCharacteristicTime, 1);
}

TEST_F(FastPairGattServiceClientTest, WriteKeyBasedRequestError) {
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 0);
  SetKeyBasedWriteError();
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicGattError, 1);
}

TEST_F(FastPairGattServiceClientTest, WriteKeyBasedRequestTimeout) {
  SetWriteRequestTimeout();
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_TRUE(ServiceIsSet());
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kKeyBasedPairingResponseTimeout);
}
TEST_F(FastPairGattServiceClientTest, WritePasskeyRequest) {
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 0);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerPasskeyGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  histogram_tester().ExpectTotalCount(kNotifyPasskeyCharacteristicTime, 1);
}

TEST_F(FastPairGattServiceClientTest, WritePasskeyRequestError) {
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  SetPasskeyWriteError();
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(),
            PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 1);
}

TEST_F(FastPairGattServiceClientTest, WritePasskeyRequestTimeout) {
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
  SetWritePasskeyTimeout();
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToPasskey();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), PairFailure::kPasskeyResponseTimeout);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicGattError, 0);
}

TEST_F(FastPairGattServiceClientTest, WriteAccountKey) {
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), absl::nullopt);
  WriteAccountKey();
  EXPECT_EQ(GetAccountKeyCallback(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 1);
}

TEST_F(FastPairGattServiceClientTest, WriteAccountKeyFailure) {
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      0);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
  SetAccountKeyCharacteristicWriteError(true);
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_EQ(GetInitializedCallbackResult(), absl::nullopt);
  EXPECT_TRUE(ServiceIsSet());
  WriteRequestToKeyBased();
  TriggerKeyBasedGattChanged();
  EXPECT_EQ(GetWriteCallbackResult(), absl::nullopt);
  WriteAccountKey();
  EXPECT_NE(GetAccountKeyCallback(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyCharacteristicGattError,
                                      1);
  histogram_tester().ExpectTotalCount(kWriteAccountKeyTimeMetric, 0);
}

}  // namespace quick_pair
}  // namespace ash
