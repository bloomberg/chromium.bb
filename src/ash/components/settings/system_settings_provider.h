// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
#define ASH_COMPONENTS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_

#include <memory>
#include <string>

#include "ash/components/settings/cros_settings_provider.h"
#include "ash/components/settings/timezone_settings.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

class COMPONENT_EXPORT(ASH_SETTINGS) SystemSettingsProvider
    : public CrosSettingsProvider,
      public system::TimezoneSettings::Observer {
 public:
  SystemSettingsProvider();
  explicit SystemSettingsProvider(const NotifyObserversCallback& notify_cb);

  SystemSettingsProvider(const SystemSettingsProvider&) = delete;
  SystemSettingsProvider& operator=(const SystemSettingsProvider&) = delete;

  ~SystemSettingsProvider() override;

  // CrosSettingsProvider implementation.
  const base::Value* Get(const std::string& path) const override;
  TrustedStatus PrepareTrustedValues(base::OnceClosure* callback) override;
  bool HandlesSetting(const std::string& path) const override;

  // TimezoneSettings::Observer implementation.
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  // Code common to both constructors.
  void Init();

  std::unique_ptr<base::Value> timezone_value_;
  std::unique_ptr<base::Value> per_user_timezone_enabled_value_;
  std::unique_ptr<base::Value> fine_grained_time_zone_enabled_value_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::SystemSettingsProvider;
}  // namespace chromeos

#endif  // ASH_COMPONENTS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
