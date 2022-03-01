// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"

#include "ash/components/settings/cros_settings_provider.h"
#include "chrome/browser/ash/settings/cros_settings.h"

namespace reporting {
base::CallbackListSubscription CrosReportingSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  return ash::CrosSettings::Get()->AddSettingsObserver(path,
                                                       std::move(callback));
}

bool CrosReportingSettings::PrepareTrustedValues(base::OnceClosure callback) {
  return ash::CrosSettings::Get()->PrepareTrustedValues(std::move(callback)) ==
         ash::CrosSettingsProvider::TRUSTED;
}

bool CrosReportingSettings::GetBoolean(const std::string& path,
                                       bool* out_value) const {
  return ash::CrosSettings::Get()->GetBoolean(path, out_value);
}

bool CrosReportingSettings::GetInteger(const std::string& path,
                                       int* out_value) const {
  return ash::CrosSettings::Get()->GetInteger(path, out_value);
}
}  // namespace reporting
