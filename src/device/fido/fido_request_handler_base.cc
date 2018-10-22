// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_request_handler_base.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/ble_adapter_power_manager.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

// PlatformAuthenticatorInfo --------------------------

PlatformAuthenticatorInfo::PlatformAuthenticatorInfo(
    std::unique_ptr<FidoAuthenticator> authenticator_,
    bool has_recognized_mac_touch_id_credential_)
    : authenticator(std::move(authenticator_)),
      has_recognized_mac_touch_id_credential(
          has_recognized_mac_touch_id_credential_) {}
PlatformAuthenticatorInfo::PlatformAuthenticatorInfo(
    PlatformAuthenticatorInfo&&) = default;
PlatformAuthenticatorInfo& PlatformAuthenticatorInfo::operator=(
    PlatformAuthenticatorInfo&&) = default;
PlatformAuthenticatorInfo::~PlatformAuthenticatorInfo() = default;

// FidoRequestHandlerBase::TransportAvailabilityInfo --------------------------

FidoRequestHandlerBase::TransportAvailabilityInfo::TransportAvailabilityInfo() =
    default;

FidoRequestHandlerBase::TransportAvailabilityInfo::TransportAvailabilityInfo(
    const TransportAvailabilityInfo& data) = default;

FidoRequestHandlerBase::TransportAvailabilityInfo&
FidoRequestHandlerBase::TransportAvailabilityInfo::operator=(
    const TransportAvailabilityInfo& other) = default;

FidoRequestHandlerBase::TransportAvailabilityInfo::
    ~TransportAvailabilityInfo() = default;

// FidoRequestHandlerBase::TransportAvailabilityObserver ----------------------

FidoRequestHandlerBase::TransportAvailabilityObserver::
    ~TransportAvailabilityObserver() = default;

// FidoRequestHandlerBase -----------------------------------------------------

FidoRequestHandlerBase::FidoRequestHandlerBase(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& available_transports)
    : weak_factory_(this) {
  // The number of times |notify_observer_callback_| needs to be invoked before
  // Observer::OnTransportAvailabilityEnumerated is dispatched. Essentially this
  // is used to wait until all the parts of |transport_availability_info_| are
  // filled out; the |notify_observer_callback_| is invoked once for each part
  // once that part is ready, namely:
  //
  //   1) Once the platform authenticator related fields are filled out.
  //   2) [Optionally, if BLE or caBLE enabled] if Bluetooth adapter is present.
  //
  // On top of that, we wait for (3) an invocation that happens when the
  // |observer_| is set, so that OnTransportAvailabilityEnumerated is never
  // called before the observer is set.
  size_t transport_info_callback_count = 1u + 0u + 1u;

  for (const auto transport : available_transports) {
    // Construction of CaBleDiscovery is handled by the implementing class as it
    // requires an extension passed on from the relying party.
    if (transport == FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy)
      continue;

    if (transport == FidoTransportProtocol::kInternal) {
      // Platform authenticator availability is always indicated by
      // |AuthenticatorImpl|.
      continue;
    }

    auto discovery = FidoDiscovery::Create(transport, connector);
    if (discovery == nullptr) {
      // This can occur in tests when a ScopedVirtualU2fDevice is in effect and
      // HID transports are not configured.
      continue;
    }

    discovery->set_observer(this);
    discoveries_.push_back(std::move(discovery));
  }

  if (base::ContainsKey(
          available_transports,
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy) ||
      base::ContainsKey(available_transports,
                        FidoTransportProtocol::kBluetoothLowEnergy)) {
    ++transport_info_callback_count;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FidoRequestHandlerBase::ConstructBleAdapterPowerManager,
                       weak_factory_.GetWeakPtr()));
  }

  transport_availability_info_.available_transports = available_transports;
  notify_observer_callback_ = base::BarrierClosure(
      transport_info_callback_count,
      base::BindOnce(
          &FidoRequestHandlerBase::NotifyObserverTransportAvailability,
          weak_factory_.GetWeakPtr()));
}

FidoRequestHandlerBase::~FidoRequestHandlerBase() = default;

void FidoRequestHandlerBase::StartAuthenticatorRequest(
    const std::string& authenticator_id) {
  auto authenticator = active_authenticators_.find(authenticator_id);
  if (authenticator == active_authenticators_.end())
    return;

  InitializeAuthenticatorAndDispatchRequest(authenticator->second.get());
}

void FidoRequestHandlerBase::CancelOngoingTasks(
    base::StringPiece exclude_device_id) {
  for (auto task_it = active_authenticators_.begin();
       task_it != active_authenticators_.end();) {
    DCHECK(!task_it->first.empty());
    if (task_it->first != exclude_device_id) {
      DCHECK(task_it->second);
      task_it->second->Cancel();
      task_it = active_authenticators_.erase(task_it);
    } else {
      ++task_it;
    }
  }
}

void FidoRequestHandlerBase::OnBluetoothAdapterEnumerated(bool is_present,
                                                          bool is_powered_on,
                                                          bool can_power_on) {
  if (!is_present) {
    transport_availability_info_.available_transports.erase(
        FidoTransportProtocol::kBluetoothLowEnergy);
    transport_availability_info_.available_transports.erase(
        FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  }

  transport_availability_info_.is_ble_powered = is_powered_on;
  transport_availability_info_.can_power_on_ble_adapter = can_power_on;
  DCHECK(notify_observer_callback_);
  notify_observer_callback_.Run();
}

void FidoRequestHandlerBase::OnBluetoothAdapterPowerChanged(
    bool is_powered_on) {
  transport_availability_info_.is_ble_powered = is_powered_on;

  if (observer_)
    observer_->BluetoothAdapterPowerChanged(is_powered_on);
}

void FidoRequestHandlerBase::PowerOnBluetoothAdapter() {
  if (!bluetooth_power_manager_)
    return;

  bluetooth_power_manager_->SetAdapterPower(true /* set_power_on */);
}

base::WeakPtr<FidoRequestHandlerBase> FidoRequestHandlerBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FidoRequestHandlerBase::Start() {
  for (const auto& discovery : discoveries_)
    discovery->Start();
}

void FidoRequestHandlerBase::DeviceAdded(FidoDiscovery* discovery,
                                         FidoDevice* device) {
  DCHECK(!base::ContainsKey(active_authenticators(), device->GetId()));
  AddAuthenticator(CreateAuthenticatorFromDevice(device));
}

std::unique_ptr<FidoDeviceAuthenticator>
FidoRequestHandlerBase::CreateAuthenticatorFromDevice(FidoDevice* device) {
  return std::make_unique<FidoDeviceAuthenticator>(device);
}

void FidoRequestHandlerBase::DeviceRemoved(FidoDiscovery* discovery,
                                           FidoDevice* device) {
  // Device connection has been lost or device has already been removed.
  // Thus, calling CancelTask() is not necessary. Also, below
  // ongoing_tasks_.erase() will have no effect for the devices that have been
  // already removed due to processing error or due to invocation of
  // CancelOngoingTasks().
  DCHECK(device);
  active_authenticators_.erase(device->GetId());

  if (observer_)
    observer_->FidoAuthenticatorRemoved(device->GetId());
}

void FidoRequestHandlerBase::AddAuthenticator(
    std::unique_ptr<FidoAuthenticator> authenticator) {
  DCHECK(authenticator &&
         !base::ContainsKey(active_authenticators(), authenticator->GetId()));
  FidoAuthenticator* authenticator_ptr = authenticator.get();
  active_authenticators_.emplace(authenticator->GetId(),
                                 std::move(authenticator));

  // If |observer_| exists, dispatching request to |authenticator_ptr| is
  // delegated to |observer_|. Else, dispatch request to |authenticator_ptr|
  // immediately.
  bool embedder_controls_dispatch = false;
  if (observer_) {
    embedder_controls_dispatch =
        observer_->EmbedderControlsAuthenticatorDispatch(*authenticator_ptr);
    observer_->FidoAuthenticatorAdded(*authenticator_ptr);
  }

  if (!embedder_controls_dispatch) {
    // Post |InitializeAuthenticatorAndDispatchRequest| into its own task. This
    // avoids hairpinning, even if the authenticator immediately invokes the
    // request callback.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FidoRequestHandlerBase::InitializeAuthenticatorAndDispatchRequest,
            GetWeakPtr(), authenticator_ptr));
  }
}

void FidoRequestHandlerBase::SetPlatformAuthenticatorOrMarkUnavailable(
    base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info) {
  if (platform_authenticator_info &&
      base::ContainsKey(transport_availability_info_.available_transports,
                        FidoTransportProtocol::kInternal)) {
    DCHECK(platform_authenticator_info->authenticator);
    DCHECK(
        (platform_authenticator_info->authenticator->AuthenticatorTransport() ==
         FidoTransportProtocol::kInternal));
    transport_availability_info_.has_recognized_mac_touch_id_credential =
        platform_authenticator_info->has_recognized_mac_touch_id_credential;
    AddAuthenticator(std::move(platform_authenticator_info->authenticator));
  } else {
    transport_availability_info_.available_transports.erase(
        FidoTransportProtocol::kInternal);
  }

  DCHECK(notify_observer_callback_);
  notify_observer_callback_.Run();
}

void FidoRequestHandlerBase::NotifyObserverTransportAvailability() {
  DCHECK(observer_);
  observer_->OnTransportAvailabilityEnumerated(transport_availability_info_);
}

void FidoRequestHandlerBase::InitializeAuthenticatorAndDispatchRequest(
    FidoAuthenticator* authenticator) {
  authenticator->InitializeAuthenticator(
      base::BindOnce(&FidoRequestHandlerBase::DispatchRequest,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void FidoRequestHandlerBase::ConstructBleAdapterPowerManager() {
  bluetooth_power_manager_ = std::make_unique<BleAdapterPowerManager>(this);
}

}  // namespace device
