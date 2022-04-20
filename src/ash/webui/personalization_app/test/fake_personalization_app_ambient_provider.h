// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_AMBIENT_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_AMBIENT_PROVIDER_H_

#include "ash/constants/ambient_animation_theme.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"

#include <stdint.h>

#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {
namespace personalization_app {

class FakePersonalizationAppAmbientProvider
    : public PersonalizationAppAmbientProvider {
 public:
  explicit FakePersonalizationAppAmbientProvider(content::WebUI* web_ui);

  FakePersonalizationAppAmbientProvider(
      const FakePersonalizationAppAmbientProvider&) = delete;
  FakePersonalizationAppAmbientProvider& operator=(
      const FakePersonalizationAppAmbientProvider&) = delete;

  ~FakePersonalizationAppAmbientProvider() override;

  // PersonalizationAppAmbientProvider:
  void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::AmbientProvider>
          receiver) override;

  // personalization_app::mojom::AmbientProvider
  void IsAmbientModeEnabled(IsAmbientModeEnabledCallback callback) override;
  void SetAmbientObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
          observer) override {}
  void SetAmbientModeEnabled(bool enabled) override {}
  void SetAnimationTheme(ash::AmbientAnimationTheme animation_theme) override {}
  void SetTopicSource(ash::AmbientModeTopicSource topic_source) override {}
  void SetTemperatureUnit(
      ash::AmbientModeTemperatureUnit temperature_unit) override {}
  void SetAlbumSelected(const std::string& id,
                        ash::AmbientModeTopicSource topic_source,
                        bool selected) override {}
  void SetPageViewed() override {}

 private:
  mojo::Receiver<ash::personalization_app::mojom::AmbientProvider>
      ambient_receiver_{this};
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_FAKE_PERSONALIZATION_APP_AMBIENT_PROVIDER_H_
