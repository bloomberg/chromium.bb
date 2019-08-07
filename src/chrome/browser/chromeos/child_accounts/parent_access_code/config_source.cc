// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/config_source.h"

namespace chromeos {
namespace parent_access {

ConfigSource::ConfigSet::ConfigSet() = default;
ConfigSource::ConfigSet::~ConfigSet() = default;
ConfigSource::ConfigSet::ConfigSet(ConfigSet&&) = default;
ConfigSource::ConfigSet& ConfigSource::ConfigSet::operator=(ConfigSet&&) =
    default;

ConfigSource::Observer::Observer() = default;
ConfigSource::Observer::~Observer() = default;

ConfigSource::ConfigSource() = default;
ConfigSource::~ConfigSource() = default;

void ConfigSource::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfigSource::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ConfigSource::NotifyConfigChanged(const ConfigSet& configs) {
  for (auto& observer : observers_)
    observer.OnConfigChanged(configs);
}

}  // namespace parent_access
}  // namespace chromeos
