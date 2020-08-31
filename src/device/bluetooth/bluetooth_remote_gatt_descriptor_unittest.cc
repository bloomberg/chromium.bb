// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(USE_CAST_BLUETOOTH_ADAPTER)
#include "device/bluetooth/test/bluetooth_test_cast.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif defined(OS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

namespace device {

class BluetoothRemoteGattDescriptorTest :
#if defined(OS_WIN)
    public BluetoothTestWinrt {
#else
    public BluetoothTest {
#endif
 public:
  // Creates adapter_, device_, service_, characteristic_,
  // descriptor1_, & descriptor2_.
  void FakeDescriptorBoilerplate() {
    InitWithFakeAdapter();
    StartLowEnergyDiscoverySession();
    device_ = SimulateLowEnergyDevice(3);
    device_->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                  GetConnectErrorCallback(Call::NOT_EXPECTED));
    SimulateGattConnection(device_);
    base::RunLoop().RunUntilIdle();
    SimulateGattServicesDiscovered(
        device_, std::vector<std::string>({kTestUUIDGenericAccess}));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(1u, device_->GetGattServices().size());
    service_ = device_->GetGattServices()[0];
    SimulateGattCharacteristic(service_, kTestUUIDDeviceName, 0);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(1u, service_->GetCharacteristics().size());
    characteristic_ = service_->GetCharacteristics()[0];
    SimulateGattDescriptor(characteristic_,
                           kTestUUIDCharacteristicUserDescription);
    SimulateGattDescriptor(characteristic_,
                           kTestUUIDClientCharacteristicConfiguration);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(2u, characteristic_->GetDescriptors().size());
    descriptor1_ = characteristic_->GetDescriptors()[0];
    descriptor2_ = characteristic_->GetDescriptors()[1];
    ResetEventCounts();
  }

  BluetoothDevice* device_ = nullptr;
  BluetoothRemoteGattService* service_ = nullptr;
  BluetoothRemoteGattCharacteristic* characteristic_ = nullptr;
  BluetoothRemoteGattDescriptor* descriptor1_ = nullptr;
  BluetoothRemoteGattDescriptor* descriptor2_ = nullptr;
};

#if defined(OS_WIN)
using BluetoothRemoteGattDescriptorTestWinrtOnly =
    BluetoothRemoteGattDescriptorTest;
#endif

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GetIdentifier GetIdentifier
#else
#define MAYBE_GetIdentifier DISABLED_GetIdentifier
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, GetIdentifier) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_GetIdentifier) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify that descriptors on them have distinct IDs.
  BluetoothDevice* device1 = SimulateLowEnergyDevice(3);
  BluetoothDevice* device2 = SimulateLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  device2->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);
  base::RunLoop().RunUntilIdle();

  // 3 services (all with same UUID).
  //   1 on the first device (to test characteristic instances across devices).
  //   2 on the second device (to test same device, multiple service instances).
  SimulateGattServicesDiscovered(
      device1, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  SimulateGattServicesDiscovered(
      device2, std::vector<std::string>(
                   {kTestUUIDGenericAccess, kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service1 = device1->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device2->GetGattServices()[0];
  BluetoothRemoteGattService* service3 = device2->GetGattServices()[1];
  // 6 characteristics (same UUID), 2 on each service.
  SimulateGattCharacteristic(service1, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service1, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service2, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service2, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service3, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service3, kTestUUIDDeviceName, /* properties */ 0);
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattCharacteristic* char1 = service1->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char2 = service1->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char3 = service2->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char4 = service2->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char5 = service3->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char6 = service3->GetCharacteristics()[1];
  // 6 descriptors (same UUID), 1 on each characteristic
  // TODO(576900) Test multiple descriptors with same UUID on one
  // characteristic.
  SimulateGattDescriptor(char1, kTestUUIDCharacteristicUserDescription);
  SimulateGattDescriptor(char2, kTestUUIDCharacteristicUserDescription);
  SimulateGattDescriptor(char3, kTestUUIDCharacteristicUserDescription);
  SimulateGattDescriptor(char4, kTestUUIDCharacteristicUserDescription);
  SimulateGattDescriptor(char5, kTestUUIDCharacteristicUserDescription);
  SimulateGattDescriptor(char6, kTestUUIDCharacteristicUserDescription);
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattDescriptor* desc1 = char1->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc2 = char2->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc3 = char3->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc4 = char4->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc5 = char5->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc6 = char6->GetDescriptors()[0];

  // All IDs are unique.
  EXPECT_NE(desc1->GetIdentifier(), desc2->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc3->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc2->GetIdentifier(), desc3->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc3->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc3->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc3->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc4->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc4->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc5->GetIdentifier(), desc6->GetIdentifier());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GetUUID GetUUID
#else
#define MAYBE_GetUUID DISABLED_GetUUID
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, GetUUID) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_GetUUID) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, device->GetGattServices().size());
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  SimulateGattCharacteristic(service, kTestUUIDDeviceName,
                             /* properties */ 0);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, service->GetCharacteristics().size());
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristics()[0];

  // Create 2 descriptors.
  BluetoothUUID uuid1(kTestUUIDCharacteristicUserDescription);
  BluetoothUUID uuid2(kTestUUIDClientCharacteristicConfiguration);
  SimulateGattDescriptor(characteristic,
                         kTestUUIDCharacteristicUserDescription);
  SimulateGattDescriptor(characteristic,
                         kTestUUIDClientCharacteristicConfiguration);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, characteristic->GetDescriptors().size());
  BluetoothRemoteGattDescriptor* descriptor1 =
      characteristic->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* descriptor2 =
      characteristic->GetDescriptors()[1];

  // Swap as needed to have descriptor1 be the one with uuid1.
  if (descriptor2->GetUUID() == uuid1)
    std::swap(descriptor1, descriptor2);

  EXPECT_EQ(uuid1, descriptor1->GetUUID());
  EXPECT_EQ(uuid2, descriptor2->GetUUID());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteDescriptor_Empty ReadRemoteDescriptor_Empty
#else
#define MAYBE_ReadRemoteDescriptor_Empty DISABLED_ReadRemoteDescriptor_Empty
#endif
// Tests ReadRemoteDescriptor and GetValue with empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, ReadRemoteDescriptor_Empty) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_ReadRemoteDescriptor_Empty) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();

  // Duplicate read reported from OS shouldn't cause a problem:
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, descriptor1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteDescriptor_Empty WriteRemoteDescriptor_Empty
#else
#define MAYBE_WriteRemoteDescriptor_Empty DISABLED_WriteRemoteDescriptor_Empty
#endif
// Tests WriteRemoteDescriptor with empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       WriteRemoteDescriptor_Empty) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_WriteRemoteDescriptor_Empty) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);
  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();

  // Duplicate write reported from OS shouldn't cause a problem:
  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty_vector, last_write_value_);
}

#if defined(OS_ANDROID)
#define MAYBE_ReadRemoteDescriptor_AfterDeleted \
  ReadRemoteDescriptor_AfterDeleted
#else
#define MAYBE_ReadRemoteDescriptor_AfterDeleted \
  DISABLED_ReadRemoteDescriptor_AfterDeleted
#endif
// Tests ReadRemoteDescriptor completing after Chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_ReadRemoteDescriptor_AfterDeleted) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberDescriptorForSubsequentAction(descriptor1_);
  DeleteDevice(device_);  // TODO(576906) delete only the descriptor.

  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(/* use remembered descriptor */ nullptr,
                             empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE("Did not crash!");
}

#if defined(OS_ANDROID)
#define MAYBE_WriteRemoteDescriptor_AfterDeleted \
  WriteRemoteDescriptor_AfterDeleted
#else
#define MAYBE_WriteRemoteDescriptor_AfterDeleted \
  DISABLED_WriteRemoteDescriptor_AfterDeleted
#endif
// Tests WriteRemoteDescriptor completing after Chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_WriteRemoteDescriptor_AfterDeleted) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberDescriptorForSubsequentAction(descriptor1_);
  DeleteDevice(device_);  // TODO(576906) delete only the descriptor.

  SimulateGattDescriptorWrite(/* use remembered descriptor */ nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE("Did not crash!");
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteDescriptor ReadRemoteDescriptor
#else
#define MAYBE_ReadRemoteDescriptor DISABLED_ReadRemoteDescriptor
#endif
// Tests ReadRemoteDescriptor and GetValue with non-empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, ReadRemoteDescriptor) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_ReadRemoteDescriptor) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  SimulateGattDescriptorRead(descriptor1_, test_vector);
  base::RunLoop().RunUntilIdle();

  // Duplicate read reported from OS shouldn't cause a problem:
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, descriptor1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteDescriptor WriteRemoteDescriptor
#else
#define MAYBE_WriteRemoteDescriptor DISABLED_WriteRemoteDescriptor
#endif
// Tests WriteRemoteDescriptor with non-empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, WriteRemoteDescriptor) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_WriteRemoteDescriptor) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  descriptor1_->WriteRemoteDescriptor(test_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);

  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_vector, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteDescriptor_Twice ReadRemoteDescriptor_Twice
#else
#define MAYBE_ReadRemoteDescriptor_Twice DISABLED_ReadRemoteDescriptor_Twice
#endif
// Tests ReadRemoteDescriptor and GetValue multiple times.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, ReadRemoteDescriptor_Twice) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_ReadRemoteDescriptor_Twice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  SimulateGattDescriptorRead(descriptor1_, test_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, descriptor1_->GetValue());

  // Read again, with different value:
  ResetEventCounts();
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, descriptor1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteDescriptor_Twice WriteRemoteDescriptor_Twice
#else
#define MAYBE_WriteRemoteDescriptor_Twice DISABLED_WriteRemoteDescriptor_Twice
#endif
// Tests WriteRemoteDescriptor multiple times.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       WriteRemoteDescriptor_Twice) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_WriteRemoteDescriptor_Twice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  descriptor1_->WriteRemoteDescriptor(test_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);

  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_write_value_);

  // Write again, with different value:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);
  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteDescriptor_MultipleDescriptors \
  ReadRemoteDescriptor_MultipleDescriptors
#else
#define MAYBE_ReadRemoteDescriptor_MultipleDescriptors \
  DISABLED_ReadRemoteDescriptor_MultipleDescriptors
#endif
// Tests ReadRemoteDescriptor on two descriptors.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       ReadRemoteDescriptor_MultipleDescriptors) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_ReadRemoteDescriptor_MultipleDescriptors) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor2_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(2, gatt_read_descriptor_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  SimulateGattDescriptorRead(descriptor1_, test_vector1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_vector1, last_read_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  SimulateGattDescriptorRead(descriptor2_, test_vector2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_vector2, last_read_value_);

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector1, descriptor1_->GetValue());
  EXPECT_EQ(test_vector2, descriptor2_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteDescriptor_MultipleDescriptors \
  WriteRemoteDescriptor_MultipleDescriptors
#else
#define MAYBE_WriteRemoteDescriptor_MultipleDescriptors \
  DISABLED_WriteRemoteDescriptor_MultipleDescriptors
#endif
// Tests WriteRemoteDescriptor on two descriptors.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       WriteRemoteDescriptor_MultipleDescriptors) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_WriteRemoteDescriptor_MultipleDescriptors) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  descriptor1_->WriteRemoteDescriptor(test_vector1, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(test_vector1, last_write_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  descriptor2_->WriteRemoteDescriptor(test_vector2, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(test_vector2, last_write_value_);

  EXPECT_EQ(2, gatt_write_descriptor_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  SimulateGattDescriptorWrite(descriptor1_);
  SimulateGattDescriptorWrite(descriptor2_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadError ReadError
#else
#define MAYBE_ReadError DISABLED_ReadError
#endif
// Tests ReadRemoteDescriptor asynchronous error.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, ReadError) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_ReadError) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));
  SimulateGattDescriptorReadError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattDescriptorReadError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteError WriteError
#else
#define MAYBE_WriteError DISABLED_WriteError
#endif
// Tests WriteRemoteDescriptor asynchronous error.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly, WriteError) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_WriteError) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattDescriptorWriteError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattDescriptorWriteError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_ReadSynchronousError ReadSynchronousError
#else
#define MAYBE_ReadSynchronousError DISABLED_ReadSynchronousError
#endif
// Tests ReadRemoteDescriptor synchronous error.
// Test not relevant for macOS since descriptor read cannot generate
// synchronous error.
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_ReadSynchronousError) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  SimulateGattDescriptorReadWillFailSynchronouslyOnce(descriptor1_);
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_read_descriptor_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID)
#define MAYBE_WriteSynchronousError WriteSynchronousError
#else
#define MAYBE_WriteSynchronousError DISABLED_WriteSynchronousError
#endif
// Tests WriteRemoteDescriptor synchronous error.
// Test not relevant for macOS since descriptor write cannot generate
// synchronous error.
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_WriteSynchronousError) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  SimulateGattDescriptorWriteWillFailSynchronouslyOnce(descriptor1_);
  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_write_descriptor_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);
  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteDescriptor_ReadPending ReadRemoteDescriptor_ReadPending
#else
#define MAYBE_ReadRemoteDescriptor_ReadPending \
  DISABLED_ReadRemoteDescriptor_ReadPending
#endif
// Tests ReadRemoteDescriptor error with a pending read operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       ReadRemoteDescriptor_ReadPending) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_ReadRemoteDescriptor_ReadPending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteDescriptor_WritePending \
  WriteRemoteDescriptor_WritePending
#else
#define MAYBE_WriteRemoteDescriptor_WritePending \
  DISABLED_WriteRemoteDescriptor_WritePending
#endif
// Tests WriteRemoteDescriptor error with a pending write operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       WriteRemoteDescriptor_WritePending) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_WriteRemoteDescriptor_WritePending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteDescriptor_WritePending \
  ReadRemoteDescriptor_WritePending
#else
#define MAYBE_ReadRemoteDescriptor_WritePending \
  DISABLED_ReadRemoteDescriptor_WritePending
#endif
// Tests ReadRemoteDescriptor error with a pending write operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       ReadRemoteDescriptor_WritePending) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_ReadRemoteDescriptor_WritePending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattDescriptorWrite(descriptor1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteDescriptor_ReadPending \
  WriteRemoteDescriptor_ReadPending
#else
#define MAYBE_WriteRemoteDescriptor_ReadPending \
  DISABLED_WriteRemoteDescriptor_ReadPending
#endif
// Tests WriteRemoteDescriptor error with a pending Read operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattDescriptorTestWinrtOnly,
       WriteRemoteDescriptor_ReadPending) {
#else
TEST_F(BluetoothRemoteGattDescriptorTest,
       MAYBE_WriteRemoteDescriptor_ReadPending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID)
#define MAYBE_ReadDuringDisconnect ReadDuringDisconnect
#else
#define MAYBE_ReadDuringDisconnect DISABLED_ReadDuringDisconnect
#endif
// Tests that read requests after a device disconnects but before the
// disconnect task runs do not result in a crash.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_ReadDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  SimulateGattDisconnection(device_);
  // Don't run the disconnect task.
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     // TODO(crbug.com/621901): Expect an error.
                                     GetGattErrorCallback(Call::NOT_EXPECTED));

  base::RunLoop().RunUntilIdle();
  // TODO(crbug.com/621901): Test error callback was called.
}

#if defined(OS_ANDROID)
#define MAYBE_WriteDuringDisconnect WriteDuringDisconnect
#else
#define MAYBE_WriteDuringDisconnect DISABLED_WriteDuringDisconnect
#endif
// Tests that write requests after a device disconnects but before the
// disconnect task runs do not result in a crash.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattDescriptorTest, MAYBE_WriteDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  SimulateGattDisconnection(device_);
  // Don't run the disconnect task.
  descriptor1_->WriteRemoteDescriptor(
      std::vector<uint8_t>(), GetCallback(Call::NOT_EXPECTED),
      // TODO(crbug.com/621901): Expect an error.
      GetGattErrorCallback(Call::NOT_EXPECTED));

  base::RunLoop().RunUntilIdle();
  // TODO(crbug.com/621901): Test that an error was returned.
}

#if defined(OS_MACOSX)
// Tests NSString for descriptor value for macOS.
// https://developer.apple.com/reference/corebluetooth/cbdescriptor
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_NSString) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);

  std::string test_string = "Hello";
  SimulateGattDescriptorReadNSStringMac(descriptor1_, test_string);
  base::RunLoop().RunUntilIdle();

  std::vector<uint8_t> test_vector(test_string.begin(), test_string.end());
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, descriptor1_->GetValue());
}

// Tests NSNumber for descriptor value for macOS.
// https://developer.apple.com/reference/corebluetooth/cbdescriptor
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_NSNumber) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);

  const short test_number = 0x1234;
  SimulateGattDescriptorReadNSNumberMac(descriptor1_, test_number);
  base::RunLoop().RunUntilIdle();

  uint8_t values[] = {0x34, 0x12};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, descriptor1_->GetValue());
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
INSTANTIATE_TEST_SUITE_P(
    All,
    BluetoothRemoteGattDescriptorTestWinrtOnly,
    ::testing::Values(true));
#endif  // defined(OS_WIN)

}  // namespace device
