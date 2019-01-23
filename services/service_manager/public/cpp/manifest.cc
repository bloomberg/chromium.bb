// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"

#include "build/build_config.h"

namespace service_manager {

Manifest::Manifest() = default;
Manifest::Manifest(const Manifest&) = default;
Manifest::Manifest(Manifest&&) = default;
Manifest::~Manifest() = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest& Manifest::operator=(Manifest&&) = default;

Manifest::Options::Options() = default;
Manifest::Options::Options(const Options&) = default;
Manifest::Options::Options(Options&&) = default;
Manifest::Options::~Options() = default;
Manifest::Options& Manifest::Options::operator=(Options&&) = default;
Manifest::Options& Manifest::Options::operator=(const Options&) = default;

Manifest::ExposedCapability::ExposedCapability() = default;
Manifest::ExposedCapability::ExposedCapability(const ExposedCapability&) =
    default;
Manifest::ExposedCapability::ExposedCapability(ExposedCapability&&) = default;
Manifest::ExposedCapability::ExposedCapability(
    const std::string& capability_name,
    std::set<const char*> interface_names)
    : capability_name(capability_name),
      interface_names(interface_names.begin(), interface_names.end()) {}
Manifest::ExposedCapability::~ExposedCapability() = default;
Manifest::ExposedCapability& Manifest::ExposedCapability::operator=(
    const ExposedCapability&) = default;
Manifest::ExposedCapability& Manifest::ExposedCapability::operator=(
    ExposedCapability&&) = default;

Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability() =
    default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    ExposedInterfaceFilterCapability&&) = default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    const ExposedInterfaceFilterCapability&) = default;
Manifest::ExposedInterfaceFilterCapability::ExposedInterfaceFilterCapability(
    const std::string& filter_name,
    const std::string& capability_name,
    std::set<const char*> interface_names)
    : filter_name(filter_name),
      capability_name(capability_name),
      interface_names(interface_names.begin(), interface_names.end()) {}
Manifest::ExposedInterfaceFilterCapability::
    ~ExposedInterfaceFilterCapability() = default;
Manifest::ExposedInterfaceFilterCapability&
Manifest::ExposedInterfaceFilterCapability::operator=(
    const ExposedInterfaceFilterCapability&) = default;
Manifest::ExposedInterfaceFilterCapability&
Manifest::ExposedInterfaceFilterCapability::operator=(
    ExposedInterfaceFilterCapability&&) = default;

Manifest& Manifest::Amend(Manifest other) {
  for (auto& other_capability : other.exposed_capabilities) {
    auto it = std::find_if(
        exposed_capabilities.begin(), exposed_capabilities.end(),
        [&other_capability](const ExposedCapability& capability) {
          return capability.capability_name == other_capability.capability_name;
        });
    if (it != exposed_capabilities.end()) {
      for (auto& name : other_capability.interface_names)
        it->interface_names.emplace(std::move(name));
    } else {
      exposed_capabilities.emplace_back(std::move(other_capability));
    }
  }

  for (auto& other_capability : other.exposed_interface_filter_capabilities) {
    auto it = std::find_if(
        exposed_interface_filter_capabilities.begin(),
        exposed_interface_filter_capabilities.end(),
        [&other_capability](
            const ExposedInterfaceFilterCapability& capability) {
          return capability.capability_name ==
                     other_capability.capability_name &&
                 capability.filter_name == other_capability.filter_name;
        });
    if (it != exposed_interface_filter_capabilities.end()) {
      for (auto& name : other_capability.interface_names)
        it->interface_names.emplace(std::move(name));
    } else {
      exposed_interface_filter_capabilities.emplace_back(
          std::move(other_capability));
    }
  }

  for (auto& capability : other.required_capabilities)
    required_capabilities.emplace_back(std::move(capability));
  for (auto& capability : other.required_interface_filter_capabilities)
    required_interface_filter_capabilities.emplace_back(std::move(capability));
  for (auto& manifest : other.packaged_services)
    packaged_services.emplace_back(std::move(manifest));
  for (auto& file_info : other.preloaded_files)
    preloaded_files.emplace_back(std::move(file_info));

  return *this;
}

}  // namespace service_manager
