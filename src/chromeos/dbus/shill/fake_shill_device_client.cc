// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_shill_device_client.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const int kSimPinMinLength = 4;
const int kSimPukRetryCount = 10;
const char kFailedMessage[] = "Failed";

void ErrorFunction(const std::string& device_path,
                   const std::string& error_name,
                   const std::string& error_message) {
  LOG(ERROR) << "Shill Error for: " << device_path << ": " << error_name
             << " : " << error_message;
}

void PostError(const std::string& error,
               ShillDeviceClient::ErrorCallback error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(error_callback), error, kFailedMessage));
}

void PostNotFoundError(ShillDeviceClient::ErrorCallback error_callback) {
  PostError(shill::kErrorResultNotFound, std::move(error_callback));
}

}  // namespace

// Matches pseudomodem.
const char FakeShillDeviceClient::kSimPuk[] = "12345678";

const char FakeShillDeviceClient::kDefaultSimPin[] = "1111";
const int FakeShillDeviceClient::kSimPinRetryCount = 3;

FakeShillDeviceClient::FakeShillDeviceClient() {}

FakeShillDeviceClient::~FakeShillDeviceClient() = default;

// ShillDeviceClient overrides.

void FakeShillDeviceClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(device_path).AddObserver(observer);
}

void FakeShillDeviceClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(device_path).RemoveObserver(observer);
}

void FakeShillDeviceClient::GetProperties(
    const dbus::ObjectPath& device_path,
    DBusMethodCallback<base::Value> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillDeviceClient::PassStubDeviceProperties,
                     weak_ptr_factory_.GetWeakPtr(), device_path,
                     std::move(callback)));
}

void FakeShillDeviceClient::SetProperty(const dbus::ObjectPath& device_path,
                                        const std::string& name,
                                        const base::Value& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  if (property_change_delay_.has_value()) {
    // Return callback immediately and set property after delay.
    std::move(callback).Run();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeShillDeviceClient::SetPropertyInternal,
                       weak_ptr_factory_.GetWeakPtr(), device_path, name,
                       value.Clone(),
                       /*callback=*/base::DoNothing(),
                       /*error_callback=*/base::DoNothing(),
                       /*notify_changed=*/true),
        *property_change_delay_);
    return;
  }

  if (simulate_inhibit_scanning_ && name == shill::kInhibitedProperty &&
      value.GetBool()) {
    SetScanning(device_path, /*is_scanning=*/true);
  }

  SetPropertyInternal(device_path, name, value, std::move(callback),
                      std::move(error_callback),
                      /*notify_changed=*/true);

  if (simulate_inhibit_scanning_ && name == shill::kInhibitedProperty &&
      !value.GetBool()) {
    SetScanning(device_path, /*is_scanning=*/false);
  }
}

void FakeShillDeviceClient::SetPropertyInternal(
    const dbus::ObjectPath& device_path,
    const std::string& name,
    const base::Value& value,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    bool notify_changed) {
  base::Value* device_properties =
      stub_devices_.FindDictKey(device_path.value());
  if (!device_properties) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  device_properties->SetKey(name, value.Clone());
  if (notify_changed) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeShillDeviceClient::NotifyObserversPropertyChanged,
                       weak_ptr_factory_.GetWeakPtr(), device_path, name));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::ClearProperty(const dbus::ObjectPath& device_path,
                                          const std::string& name,
                                          VoidDBusMethodCallback callback) {
  base::Value* device_properties =
      stub_devices_.FindDictKey(device_path.value());
  if (!device_properties) {
    PostVoidCallback(std::move(callback), false);
    return;
  }
  device_properties->RemoveKey(name);
  PostVoidCallback(std::move(callback), true);
}

void FakeShillDeviceClient::RequirePin(const dbus::ObjectPath& device_path,
                                       const std::string& pin,
                                       bool require,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  VLOG(1) << "RequirePin: " << device_path.value();
  if (!stub_devices_.FindKey(device_path.value())) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  if (!SimTryPin(device_path.value(), pin)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  shill::kErrorResultIncorrectPin, ""));
    return;
  }
  SimLockStatus status = GetSimLockStatus(device_path.value());
  status.lock_enabled = require;
  SetSimLockStatus(device_path.value(), status);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::EnterPin(const dbus::ObjectPath& device_path,
                                     const std::string& pin,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) {
  VLOG(1) << "EnterPin: " << device_path.value();
  if (!stub_devices_.FindKey(device_path.value())) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  if (!SimTryPin(device_path.value(), pin)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  shill::kErrorResultIncorrectPin, ""));
    return;
  }
  SetSimLocked(device_path.value(), false);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::UnblockPin(const dbus::ObjectPath& device_path,
                                       const std::string& puk,
                                       const std::string& pin,
                                       base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  VLOG(1) << "UnblockPin: " << device_path.value();
  if (!stub_devices_.FindKey(device_path.value())) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  if (!SimTryPuk(device_path.value(), puk)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  shill::kErrorResultIncorrectPin, ""));
    return;
  }
  if (pin.length() < kSimPinMinLength) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  shill::kErrorResultInvalidArguments, ""));
    return;
  }
  sim_pin_[device_path.value()] = pin;
  SetSimLocked(device_path.value(), false);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::ChangePin(const dbus::ObjectPath& device_path,
                                      const std::string& old_pin,
                                      const std::string& new_pin,
                                      base::OnceClosure callback,
                                      ErrorCallback error_callback) {
  VLOG(1) << "ChangePin: " << device_path.value();
  if (!stub_devices_.FindKey(device_path.value())) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  if (!SimTryPin(device_path.value(), old_pin)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  shill::kErrorResultIncorrectPin, ""));
    return;
  }
  if (new_pin.length() < kSimPinMinLength) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  shill::kErrorResultInvalidArguments, ""));
    return;
  }
  sim_pin_[device_path.value()] = new_pin;

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::Register(const dbus::ObjectPath& device_path,
                                     const std::string& network_id,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) {
  base::Value* device_properties =
      stub_devices_.FindDictKey(device_path.value());
  if (!device_properties) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  base::Value* scan_results =
      device_properties->FindKey(shill::kFoundNetworksProperty);
  if (!scan_results) {
    PostError("No Cellular scan results", std::move(error_callback));
    return;
  }
  for (auto& network : scan_results->GetList()) {
    std::string id = network.FindKey(shill::kNetworkIdProperty)->GetString();
    std::string status = id == network_id ? "current" : "available";
    network.SetKey(shill::kStatusProperty, base::Value(status));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::Reset(const dbus::ObjectPath& device_path,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) {
  if (!stub_devices_.FindKey(device_path.value())) {
    PostNotFoundError(std::move(error_callback));
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void FakeShillDeviceClient::SetUsbEthernetMacAddressSource(
    const dbus::ObjectPath& device_path,
    const std::string& source,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!stub_devices_.FindKey(device_path.value())) {
    PostNotFoundError(std::move(error_callback));
    return;
  }

  const auto error_name_iter =
      set_usb_ethernet_mac_address_source_error_names_.find(
          device_path.value());
  if (error_name_iter !=
          set_usb_ethernet_mac_address_source_error_names_.end() &&
      !error_name_iter->second.empty()) {
    PostError(error_name_iter->second, std::move(error_callback));
    return;
  }

  SetDeviceProperty(device_path.value(),
                    shill::kUsbEthernetMacAddressSourceProperty,
                    base::Value(source), /*notify_changed=*/true);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

ShillDeviceClient::TestInterface* FakeShillDeviceClient::GetTestInterface() {
  return this;
}

// ShillDeviceClient::TestInterface overrides.

void FakeShillDeviceClient::AddDevice(const std::string& device_path,
                                      const std::string& type,
                                      const std::string& name) {
  ShillManagerClient::Get()->GetTestInterface()->AddDevice(device_path);

  base::Value* properties = GetDeviceProperties(device_path);
  properties->SetKey(shill::kTypeProperty, base::Value(type));
  properties->SetKey(shill::kNameProperty, base::Value(name));
  properties->SetKey(shill::kDBusObjectProperty, base::Value(device_path));
  properties->SetKey(shill::kDBusServiceProperty,
                     base::Value(modemmanager::kModemManager1ServiceName));
  if (type == shill::kTypeCellular) {
    properties->SetKey(shill::kCellularPolicyAllowRoamingProperty,
                       base::Value(false));
  }
}

void FakeShillDeviceClient::RemoveDevice(const std::string& device_path) {
  ShillManagerClient::Get()->GetTestInterface()->RemoveDevice(device_path);
  stub_devices_.RemoveKey(device_path);
}

void FakeShillDeviceClient::ClearDevices() {
  ShillManagerClient::Get()->GetTestInterface()->ClearDevices();
  stub_devices_ = base::Value(base::Value::Type::DICTIONARY);
}

void FakeShillDeviceClient::SetDeviceProperty(const std::string& device_path,
                                              const std::string& name,
                                              const base::Value& value,
                                              bool notify_changed) {
  VLOG(1) << "SetDeviceProperty: " << device_path << ": " << name << " = "
          << value;
  SetPropertyInternal(
      dbus::ObjectPath(device_path), name, value, base::DoNothing(),
      base::BindOnce(&ErrorFunction, device_path), notify_changed);
}

std::string FakeShillDeviceClient::GetDevicePathForType(
    const std::string& type) {
  for (auto iter : stub_devices_.DictItems()) {
    if (!iter.second.is_dict())
      continue;
    const std::string* prop_type =
        iter.second.FindStringKey(shill::kTypeProperty);
    if (!prop_type || *prop_type != type)
      continue;
    return iter.first;
  }
  return std::string();
}

void FakeShillDeviceClient::SetSimLocked(const std::string& device_path,
                                         bool locked) {
  SimLockStatus status = GetSimLockStatus(device_path);
  status.type = locked ? shill::kSIMLockPin : "";
  status.retries_left = kSimPinRetryCount;
  SetSimLockStatus(device_path, status);
}

void FakeShillDeviceClient::AddCellularFoundNetwork(
    const std::string& device_path) {
  base::Value* device_properties = stub_devices_.FindDictKey(device_path);
  if (!device_properties) {
    LOG(ERROR) << "Device path not found: " << device_path;
    return;
  }
  std::string type =
      device_properties->FindKey(shill::kTypeProperty)->GetString();
  if (type != shill::kTypeCellular) {
    LOG(ERROR) << "AddCellularNetwork called for non Cellular network: "
               << device_path;
    return;
  }

  // Add a new scan result entry
  base::Value* scan_results =
      device_properties->FindKey(shill::kFoundNetworksProperty);
  if (!scan_results) {
    scan_results = device_properties->SetKey(shill::kFoundNetworksProperty,
                                             base::ListValue());
  }
  base::Value new_result(base::Value::Type::DICTIONARY);
  int idx = static_cast<int>(scan_results->GetList().size());
  new_result.SetKey(shill::kNetworkIdProperty,
                    base::Value(base::StringPrintf("network%d", idx)));
  new_result.SetKey(shill::kLongNameProperty,
                    base::Value(base::StringPrintf("Network %d", idx)));
  new_result.SetKey(shill::kTechnologyProperty, base::Value("GSM"));
  new_result.SetKey(shill::kStatusProperty, base::Value("available"));
  scan_results->Append(std::move(new_result));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeShillDeviceClient::NotifyObserversPropertyChanged,
                     weak_ptr_factory_.GetWeakPtr(),
                     dbus::ObjectPath(device_path),
                     shill::kFoundNetworksProperty));
}

void FakeShillDeviceClient::SetUsbEthernetMacAddressSourceError(
    const std::string& device_path,
    const std::string& error_name) {
  set_usb_ethernet_mac_address_source_error_names_[device_path] = error_name;
}

void FakeShillDeviceClient::SetSimulateInhibitScanning(
    bool simulate_inhibit_scanning) {
  simulate_inhibit_scanning_ = simulate_inhibit_scanning;
}

void FakeShillDeviceClient::SetPropertyChangeDelay(
    absl::optional<base::TimeDelta> time_delay) {
  property_change_delay_ = time_delay;
}

// Private Methods -------------------------------------------------------------

FakeShillDeviceClient::SimLockStatus FakeShillDeviceClient::GetSimLockStatus(
    const std::string& device_path) {
  SimLockStatus status;
  base::Value* device_properties = stub_devices_.FindDictKey(device_path);
  if (!device_properties)
    return status;
  base::Value* simlock_dict =
      device_properties->FindDictKey(shill::kSIMLockStatusProperty);
  if (!simlock_dict)
    return status;
  const std::string* type =
      simlock_dict->FindStringKey(shill::kSIMLockTypeProperty);
  if (type)
    status.type = *type;
  status.retries_left =
      simlock_dict->FindIntKey(shill::kSIMLockRetriesLeftProperty).value_or(0);
  status.lock_enabled =
      simlock_dict->FindBoolKey(shill::kSIMLockEnabledProperty).value_or(false);
  if (status.type == shill::kSIMLockPin && status.retries_left == 0)
    status.retries_left = kSimPinRetryCount;
  return status;
}

void FakeShillDeviceClient::SetSimLockStatus(const std::string& device_path,
                                             const SimLockStatus& status) {
  base::Value* device_properties = stub_devices_.FindDictKey(device_path);
  if (!device_properties) {
    NOTREACHED() << "Device not found: " << device_path;
    return;
  }

  base::Value* simlock_dict =
      device_properties->SetKey(shill::kSIMLockStatusProperty,
                                base::Value(base::Value::Type::DICTIONARY));

  simlock_dict->SetKey(shill::kSIMLockTypeProperty, base::Value(status.type));
  simlock_dict->SetKey(shill::kSIMLockRetriesLeftProperty,
                       base::Value(status.retries_left));
  simlock_dict->SetKey(shill::kSIMLockEnabledProperty,
                       base::Value(status.lock_enabled));
  NotifyObserversPropertyChanged(dbus::ObjectPath(device_path),
                                 shill::kSIMLockStatusProperty);
}

bool FakeShillDeviceClient::SimTryPin(const std::string& device_path,
                                      const std::string& pin) {
  SimLockStatus status = GetSimLockStatus(device_path);
  if (status.type == shill::kSIMLockPuk) {
    VLOG(1) << "SimTryPin called with PUK locked.";
    return false;  // PUK locked, PIN won't work.
  }
  if (pin.length() < kSimPinMinLength)
    return false;
  std::string sim_pin = sim_pin_[device_path];
  if (sim_pin.empty()) {
    sim_pin = kDefaultSimPin;
    sim_pin_[device_path] = sim_pin;
  }
  if (pin == sim_pin) {
    status.type = "";
    status.retries_left = kSimPinRetryCount;
    SetSimLockStatus(device_path, status);
    return true;
  }

  VLOG(1) << "SIM PIN: " << pin << " != " << sim_pin
          << " Retries left: " << (status.retries_left - 1);
  if (--status.retries_left <= 0) {
    status.retries_left = kSimPukRetryCount;
    status.type = shill::kSIMLockPuk;
    status.lock_enabled = true;
  }
  SetSimLockStatus(device_path, status);
  return false;
}

bool FakeShillDeviceClient::SimTryPuk(const std::string& device_path,
                                      const std::string& puk) {
  SimLockStatus status = GetSimLockStatus(device_path);
  if (status.type != shill::kSIMLockPuk) {
    VLOG(1) << "PUK Not locked";
    return true;  // Not PUK locked.
  }
  if (status.retries_left == 0) {
    VLOG(1) << "PUK: No retries left";
    return false;  // Permanently locked.
  }

  if (puk == kSimPuk) {
    status.type = "";
    status.retries_left = kSimPinRetryCount;
    SetSimLockStatus(device_path, status);
    return true;
  }

  --status.retries_left;
  VLOG(1) << "SIM PUK: " << puk << " != " << kSimPuk
          << " Retries left: " << status.retries_left;
  SetSimLockStatus(device_path, status);
  return false;
}

void FakeShillDeviceClient::PassStubDeviceProperties(
    const dbus::ObjectPath& device_path,
    DBusMethodCallback<base::Value> callback) const {
  const base::Value* device_properties =
      stub_devices_.FindDictKey(device_path.value());
  if (!device_properties) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(device_properties->Clone());
}

// Posts a task to run a void callback with status code |status|.
void FakeShillDeviceClient::PostVoidCallback(VoidDBusMethodCallback callback,
                                             bool result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeShillDeviceClient::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& device_path,
    const std::string& property) {
  std::string path = device_path.value();
  base::Value* device_properties = stub_devices_.FindDictKey(path);
  if (!device_properties) {
    LOG(ERROR) << "Notify for unknown device: " << path;
    return;
  }
  base::Value* value = device_properties->FindKey(property);
  if (!value) {
    LOG(ERROR) << "Notify for unknown property: " << path << " : " << property;
    return;
  }
  for (auto& observer : GetObserverList(device_path))
    observer.OnPropertyChanged(property, *value);
}

base::Value* FakeShillDeviceClient::GetDeviceProperties(
    const std::string& device_path) {
  base::Value* properties = stub_devices_.FindDictKey(device_path);
  if (!properties) {
    properties = stub_devices_.SetKey(
        device_path, base::Value(base::Value::Type::DICTIONARY));
  }
  return properties;
}

FakeShillDeviceClient::PropertyObserverList&
FakeShillDeviceClient::GetObserverList(const dbus::ObjectPath& device_path) {
  auto iter = observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = base::WrapUnique(observer_list);
  return *observer_list;
}

void FakeShillDeviceClient::SetScanning(const dbus::ObjectPath& device_path,
                                        bool is_scanning) {
  SetPropertyInternal(device_path, shill::kScanningProperty,
                      base::Value(is_scanning),
                      /*callback=*/base::DoNothing(),
                      /*error_callback=*/base::DoNothing(),
                      /*notify_changed=*/true);
}

}  // namespace chromeos
