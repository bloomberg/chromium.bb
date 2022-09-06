// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/fake_personalization_app_theme_provider.h"

namespace ash {
namespace personalization_app {

FakePersonalizationAppThemeProvider::FakePersonalizationAppThemeProvider(
    content::WebUI* web_ui) {}

FakePersonalizationAppThemeProvider::~FakePersonalizationAppThemeProvider() =
    default;

void FakePersonalizationAppThemeProvider::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::ThemeProvider>
        receiver) {
  theme_receiver_.reset();
  theme_receiver_.Bind(std::move(receiver));
}

void FakePersonalizationAppThemeProvider::SetThemeObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
        observer) {}

void FakePersonalizationAppThemeProvider::SetColorModePref(
    bool dark_mode_enabled) {
  return;
}

void FakePersonalizationAppThemeProvider::SetColorModeAutoScheduleEnabled(
    bool enabled) {
  return;
}

void FakePersonalizationAppThemeProvider::IsDarkModeEnabled(
    IsDarkModeEnabledCallback callback) {
  std::move(callback).Run(/*darkModeEnabled=*/false);
}

void FakePersonalizationAppThemeProvider::IsColorModeAutoScheduleEnabled(
    IsColorModeAutoScheduleEnabledCallback callback) {
  std::move(callback).Run(/*enabled=*/false);
}

}  // namespace personalization_app
}  // namespace ash
