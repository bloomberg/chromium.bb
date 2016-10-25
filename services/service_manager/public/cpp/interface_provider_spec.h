// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_H_

#include <map>
#include <set>
#include <string>
#include <unordered_map>

namespace service_manager {

using Capability = std::string;
using CapabilitySet = std::set<std::string>;
using Interface = std::string;
using InterfaceSet = std::set<std::string>;
using Name = std::string;

// See comments in
// services/service_manager/public/interfaces/interface_provider_spec.mojom for
// a description of InterfaceProviderSpec.
struct InterfaceProviderSpec {
  InterfaceProviderSpec();
  InterfaceProviderSpec(const InterfaceProviderSpec& other);
  ~InterfaceProviderSpec();
  bool operator==(const InterfaceProviderSpec& other) const;
  bool operator<(const InterfaceProviderSpec& other) const;
  std::map<Capability, InterfaceSet> provides;
  std::map<Name, CapabilitySet> requires;
};

// Map of spec name -> spec.
using InterfaceProviderSpecMap =
    std::unordered_map<std::string, InterfaceProviderSpec>;

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_SPEC_H_
