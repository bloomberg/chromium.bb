// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/capabilities.h"

#include <tuple>

namespace service_manager {

CapabilitySpec::CapabilitySpec() {}
CapabilitySpec::CapabilitySpec(const CapabilitySpec& other) = default;
CapabilitySpec::~CapabilitySpec() {}

bool CapabilitySpec::operator==(const CapabilitySpec& other) const {
  return other.provided == provided && other.required == required;
}

bool CapabilitySpec::operator<(const CapabilitySpec& other) const {
  return std::tie(provided, required) <
      std::tie(other.provided, other.required);
}

}  // namespace service_manager
