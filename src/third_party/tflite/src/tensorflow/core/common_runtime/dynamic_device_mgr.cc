/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <iterator>
#include <memory>
#include <vector>

#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/common_runtime/local_device.h"
#include "tensorflow/core/framework/device_attributes.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/util/device_name_utils.h"

namespace tensorflow {

DynamicDeviceMgr::DynamicDeviceMgr() : cpu_device_(nullptr) {}

DynamicDeviceMgr::~DynamicDeviceMgr() {
  // Release resources ahead of destroying the device manager as the resource
  // destructors (e.g. ~IteratorResource) assume devices still exist.
  for (auto& pair : dynamic_devices_) {
    pair.first->ClearResourceMgr();
  }
}

void DynamicDeviceMgr::ListDeviceAttributes(
    std::vector<DeviceAttributes>* devices) const {
  tf_shared_lock l(devices_mu_);
  devices->reserve(dynamic_devices_.size());
  for (const auto& pair : dynamic_devices_) {
    devices->emplace_back(pair.first->attributes());
  }
}

std::vector<Device*> DynamicDeviceMgr::ListDevices() const {
  tf_shared_lock l(devices_mu_);
  std::vector<Device*> devices;
  devices.reserve(dynamic_devices_.size());
  for (const auto& pair : dynamic_devices_) {
    devices.emplace_back(pair.first);
  }
  return devices;
}

string DynamicDeviceMgr::DebugString() const {
  string out;
  tf_shared_lock l(devices_mu_);
  for (const auto& pair : dynamic_devices_) {
    strings::StrAppend(&out, pair.first->name(), "\n");
  }
  return out;
}

string DynamicDeviceMgr::DeviceMappingString() const {
  string out;
  tf_shared_lock l(devices_mu_);
  for (const auto& pair : dynamic_devices_) {
    if (!pair.first->attributes().physical_device_desc().empty()) {
      strings::StrAppend(&out, pair.first->name(), " -> ",
                         pair.first->attributes().physical_device_desc(), "\n");
    }
  }
  return out;
}

Status DynamicDeviceMgr::LookupDevice(StringPiece name, Device** device) const {
  tf_shared_lock l(devices_mu_);
  auto iter = device_map_.find(string(name));
  if (iter == device_map_.end()) {
    std::vector<StringPiece> device_names;
    for (auto&& itr : device_map_) {
      device_names.push_back(itr.first);
    }
    VLOG(1) << "Unknown device: " << name
            << " all devices: " << absl::StrJoin(device_names, ", ");
    return errors::InvalidArgument(name, " unknown device.");
  }
  *device = iter->second;
  return Status::OK();
}

bool DynamicDeviceMgr::ContainsDevice(int64 device_incarnation) const {
  tf_shared_lock l(devices_mu_);
  return device_incarnation_set_.contains(device_incarnation);
}

void DynamicDeviceMgr::ClearContainers(
    gtl::ArraySlice<string> containers) const {
  Status s;
  tf_shared_lock l(devices_mu_);
  for (const auto& pair : dynamic_devices_) {
    if (containers.empty()) {
      s.Update(pair.first->resource_manager()->Cleanup(
          pair.first->resource_manager()->default_container()));
    } else {
      for (const string& c : containers) {
        s.Update(pair.first->resource_manager()->Cleanup(c));
      }
    }
    if (!s.ok()) {
      LOG(WARNING) << s;
    }
  }
}

int DynamicDeviceMgr::NumDeviceType(const string& type) const {
  tf_shared_lock l(devices_mu_);
  auto iter = device_type_counts_.find(type);
  if (iter != device_type_counts_.end()) return iter->second;
  return 0;
}

Status DynamicDeviceMgr::AddDevices(
    std::vector<std::unique_ptr<Device>> devices) {
  mutex_lock l(devices_mu_);
  for (auto& d : devices) {
    if (device_map_.find(d->name()) != device_map_.end()) {
      return errors::InvalidArgument(
          "Trying to add device ", d->name(),
          " to manager but its name conflicts with an existing device.");
    }
    // Register under the (1) full name and (2) canonical name.
    for (const string& name :
         DeviceNameUtils::GetNamesForDeviceMappings(d->parsed_name())) {
      device_map_[name] = d.get();
    }
    // Register under the (3) local name and (4) legacy local name.
    for (const string& name :
         DeviceNameUtils::GetLocalNamesForDeviceMappings(d->parsed_name())) {
      device_map_[name] = d.get();
    }
    device_type_counts_[d->device_type()]++;
    device_incarnation_set_.insert(d->attributes().incarnation());
    dynamic_devices_.emplace(d.get(), std::move(d));
  }
  return Status::OK();
}

Status DynamicDeviceMgr::RemoveDevices(std::vector<Device*> devices) {
  mutex_lock l(devices_mu_);

  for (const auto& d : devices) {
    if (d == cpu_device_) {
      TF_RETURN_IF_ERROR(
          errors::InvalidArgument("Can not remove HostCPU device ", d->name()));
    }
    auto it = dynamic_devices_.find(d);
    if (it == dynamic_devices_.end()) {
      TF_RETURN_IF_ERROR(errors::InvalidArgument("Unknown device ", d->name()));
    }
  }

  for (const auto& d : devices) {
    auto it = dynamic_devices_.find(d);

    // Clear registration of (1) full name and (2) canonical name
    for (const string& name :
         DeviceNameUtils::GetNamesForDeviceMappings(d->parsed_name())) {
      device_map_.erase(name);
    }
    // Clear registration of (3) local name and (4) legacy local name
    for (const string& name :
         DeviceNameUtils::GetLocalNamesForDeviceMappings(d->parsed_name())) {
      device_map_.erase(name);
    }
    device_type_counts_[d->device_type()]--;
    device_incarnation_set_.erase(d->attributes().incarnation());
    stale_devices_.add(std::move(it->second));
    dynamic_devices_.erase(it);
  }
  return Status::OK();
}

Status DynamicDeviceMgr::RemoveDevicesByName(
    const std::vector<string>& device_names) {
  std::vector<Device*> devices_to_remove;
  for (const string& name : device_names) {
    Device* device;
    TF_RETURN_IF_ERROR(LookupDevice(name, &device));
    devices_to_remove.emplace_back(device);
  }
  return RemoveDevices(devices_to_remove);
}

Device* DynamicDeviceMgr::HostCPU() const {
  mutex_lock l(devices_mu_);
  if (dynamic_devices_.find(cpu_device_) != dynamic_devices_.end()) {
    return cpu_device_;
  }
  cpu_device_ = nullptr;
  for (const auto& pair : dynamic_devices_) {
    if (pair.first->device_type() == DEVICE_CPU &&
        pair.first->parsed_name().id == 0) {
      cpu_device_ = pair.first;
      break;
    }
  }
  return cpu_device_;
}

}  // namespace tensorflow
