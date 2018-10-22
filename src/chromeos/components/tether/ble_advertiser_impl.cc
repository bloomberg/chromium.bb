// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertiser_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/ble_service_data_helper.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/error_tolerant_ble_advertisement_impl.h"
#include "components/cryptauth/ble/ble_advertisement_generator.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_ref.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace tether {

namespace {

const char kStubLocalDeviceId[] = "N/A";

// Instant Tethering does not make use of the "local device ID" argument, since
// all connections are from the same device.
// TODO(hansberry): Remove when SecureChannelClient migration is complete.
secure_channel::DeviceIdPair StubDeviceIdPair(
    const std::string& remote_device_id) {
  return secure_channel::DeviceIdPair(remote_device_id, kStubLocalDeviceId);
}

}  // namespace

// static
BleAdvertiserImpl::Factory* BleAdvertiserImpl::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<BleAdvertiser> BleAdvertiserImpl::Factory::NewInstance(
    secure_channel::BleServiceDataHelper* ble_service_data_helper,
    secure_channel::BleSynchronizerBase* ble_synchronizer) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(ble_service_data_helper,
                                          ble_synchronizer);
}

// static
void BleAdvertiserImpl::Factory::SetInstanceForTesting(Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<BleAdvertiser> BleAdvertiserImpl::Factory::BuildInstance(
    secure_channel::BleServiceDataHelper* ble_service_data_helper,
    secure_channel::BleSynchronizerBase* ble_synchronizer) {
  return base::WrapUnique(
      new BleAdvertiserImpl(ble_service_data_helper, ble_synchronizer));
}

BleAdvertiserImpl::AdvertisementMetadata::AdvertisementMetadata(
    const std::string& device_id,
    std::unique_ptr<cryptauth::DataWithTimestamp> service_data)
    : device_id(device_id), service_data(std::move(service_data)) {}

BleAdvertiserImpl::AdvertisementMetadata::~AdvertisementMetadata() = default;

BleAdvertiserImpl::BleAdvertiserImpl(
    secure_channel::BleServiceDataHelper* ble_service_data_helper,
    secure_channel::BleSynchronizerBase* ble_synchronizer)
    : ble_service_data_helper_(ble_service_data_helper),
      ble_synchronizer_(ble_synchronizer),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

BleAdvertiserImpl::~BleAdvertiserImpl() = default;

bool BleAdvertiserImpl::StartAdvertisingToDevice(const std::string& device_id) {
  int index_for_device = -1;
  for (size_t i = 0; i < secure_channel::kMaxConcurrentAdvertisements; ++i) {
    if (!registered_device_metadata_[i]) {
      index_for_device = i;
      break;
    }
  }

  if (index_for_device == -1) {
    PA_LOG(ERROR) << "Attempted to register a device when the maximum number "
                  << "of devices have already been registered.";
    return false;
  }

  std::unique_ptr<cryptauth::DataWithTimestamp> service_data =
      ble_service_data_helper_->GenerateForegroundAdvertisement(
          secure_channel::DeviceIdPair(device_id /* remote_device_id */,
                                       kStubLocalDeviceId));
  if (!service_data) {
    PA_LOG(WARNING) << "Error generating advertisement for device with ID "
                    << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id)
                    << ". Cannot advertise.";
    return false;
  }

  registered_device_metadata_[index_for_device].reset(
      new AdvertisementMetadata(device_id, std::move(service_data)));
  UpdateAdvertisements();

  return true;
}

bool BleAdvertiserImpl::StopAdvertisingToDevice(const std::string& device_id) {
  for (auto& metadata : registered_device_metadata_) {
    if (metadata && metadata->device_id == device_id) {
      metadata.reset();
      UpdateAdvertisements();
      return true;
    }
  }

  return false;
}

bool BleAdvertiserImpl::AreAdvertisementsRegistered() {
  for (const auto& advertisement : advertisements_) {
    if (advertisement)
      return true;
  }

  return false;
}

void BleAdvertiserImpl::SetTaskRunnerForTesting(
    scoped_refptr<base::TaskRunner> test_task_runner) {
  task_runner_ = test_task_runner;
}

void BleAdvertiserImpl::UpdateAdvertisements() {
  for (size_t i = 0; i < secure_channel::kMaxConcurrentAdvertisements; ++i) {
    std::unique_ptr<AdvertisementMetadata>& metadata =
        registered_device_metadata_[i];
    std::unique_ptr<secure_channel::ErrorTolerantBleAdvertisement>&
        advertisement = advertisements_[i];

    // If there is a registered device but no associated advertisement, create
    // the advertisement.
    if (metadata && !advertisement) {
      std::unique_ptr<cryptauth::DataWithTimestamp> service_data_copy =
          std::make_unique<cryptauth::DataWithTimestamp>(
              *metadata->service_data);
      advertisements_[i] =
          secure_channel::ErrorTolerantBleAdvertisementImpl::Factory::Get()
              ->BuildInstance(StubDeviceIdPair(metadata->device_id),
                              std::move(service_data_copy), ble_synchronizer_);
      continue;
    }

    // If there is no registered device but there is an advertisement, stop it
    // if it has not yet been stopped.
    if (!metadata && advertisement && !advertisement->HasBeenStopped()) {
      advertisement->Stop(base::Bind(&BleAdvertiserImpl::OnAdvertisementStopped,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     i /* index */));
    }
  }
}

void BleAdvertiserImpl::OnAdvertisementStopped(size_t index) {
  DCHECK(advertisements_[index] && advertisements_[index]->HasBeenStopped());
  advertisements_[index].reset();

  // Update advertisements, but do so as part of a new task in the run loop to
  // prevent the possibility of a crash. See crbug.com/776241.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BleAdvertiserImpl::UpdateAdvertisements,
                                weak_ptr_factory_.GetWeakPtr()));

  if (!AreAdvertisementsRegistered())
    NotifyAllAdvertisementsUnregistered();
}

}  // namespace tether

}  // namespace chromeos
