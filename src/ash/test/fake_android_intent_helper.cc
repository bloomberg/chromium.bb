// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/fake_android_intent_helper.h"

namespace ash {

FakeAndroidIntentHelper::FakeAndroidIntentHelper() = default;
FakeAndroidIntentHelper::~FakeAndroidIntentHelper() = default;

void FakeAndroidIntentHelper::LaunchAndroidIntent(const std::string& intent) {
  last_launched_intent_ = intent;
}

base::Optional<std::string> FakeAndroidIntentHelper::GetAndroidAppLaunchIntent(
    chromeos::assistant::mojom::AndroidAppInfoPtr app_info) {
  auto iterator = apps_.find(app_info->localized_app_name);
  if (iterator != apps_.end())
    return iterator->second;
  return base::nullopt;
}

void FakeAndroidIntentHelper::AddApp(const LocalizedAppName& name,
                                     const Intent& intent) {
  apps_[name] = intent;
}

}  // namespace ash
