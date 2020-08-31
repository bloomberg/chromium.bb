// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_configuration_observer.h"

namespace chromeos {

// NetworkConfigurationObserver::NetworkConfigurationObserver() = default;

NetworkConfigurationObserver::~NetworkConfigurationObserver() = default;

void NetworkConfigurationObserver::OnConfigurationCreated(
    const std::string& service_path,
    const std::string& guid) {}

void NetworkConfigurationObserver::OnConfigurationModified(
    const std::string& service_path,
    const std::string& guid,
    base::DictionaryValue* set_properties) {}

void NetworkConfigurationObserver::OnBeforeConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {}

void NetworkConfigurationObserver::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {}

}  // namespace chromeos
