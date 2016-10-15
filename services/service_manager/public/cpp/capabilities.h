// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CAPABILITIES_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CAPABILITIES_H_

#include <map>
#include <set>
#include <string>

namespace service_manager {

using Class = std::string;
using Classes = std::set<std::string>;
using Interface = std::string;
using Interfaces = std::set<std::string>;
using Name = std::string;

// See comments in
// services/service_manager/public/interfaces/capabilities.mojom for a
// description of CapabilitySpec.
struct CapabilitySpec {
  CapabilitySpec();
  CapabilitySpec(const CapabilitySpec& other);
  ~CapabilitySpec();
  bool operator==(const CapabilitySpec& other) const;
  bool operator<(const CapabilitySpec& other) const;
  std::map<Class, Interfaces> provided;
  std::map<Name, Classes> required;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CAPABILITIES_H_
