// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_DISPLAY_SETTINGS_MANAGER_IMPL_H_
#define CHROMECAST_UI_DISPLAY_SETTINGS_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "chromecast/ui/display_settings_manager.h"
#include "chromecast/ui/mojom/display_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromecast {

class CastWindowManager;
class ColorTemperatureAnimation;
class BrightnessAnimation;
class GammaAnimation;

#if defined(USE_AURA)
class GammaConfigurator;
#endif  // defined(USE_AURA)

namespace shell {
class CastDisplayConfigurator;
}

class DisplaySettingsManagerImpl : public DisplaySettingsManager,
                                   public mojom::DisplaySettings {
 public:
  DisplaySettingsManagerImpl(
      CastWindowManager* window_manager,
      const DisplaySettingsManager::ColorTemperatureConfig&
          color_temperature_config);
  DisplaySettingsManagerImpl(const DisplaySettingsManagerImpl&) = delete;
  DisplaySettingsManagerImpl& operator=(const DisplaySettingsManagerImpl&) =
      delete;
  ~DisplaySettingsManagerImpl() override;

  // DisplaySettingsManager implementation:
  void SetDelegate(DisplaySettingsManager::Delegate* delegate) override;
  void ResetDelegate() override;
  void SetGammaCalibration(
      const std::vector<display::GammaRampRGBEntry>& gamma) override;
  void NotifyBrightnessChanged(float new_brightness,
                               float old_brightness) override;
  void SetColorInversion(bool enable) override;
  void AddReceiver(
      mojo::PendingReceiver<mojom::DisplaySettings> receiver) override;

  // mojom::DisplaySettings implementation:
  void SetColorTemperature(float temperature) override;
  void SetColorTemperatureSmooth(float temperature,
                                 base::TimeDelta duration) override;
  void ResetColorTemperature() override;
  void SetBrightness(float brightness) override;
  void SetBrightnessSmooth(float brightness, base::TimeDelta duration) override;
  void ResetBrightness() override;
  void SetScreenOn(bool screen_on) override;

 private:
  // mojom::DisplaySettingsObserver implementation
  void AddDisplaySettingsObserver(
      mojo::PendingRemote<mojom::DisplaySettingsObserver> observer) override;

  void UpdateBrightness(base::TimeDelta duration);

  CastWindowManager* const window_manager_;
  shell::CastDisplayConfigurator* const display_configurator_;

#if defined(USE_AURA)
  const std::unique_ptr<GammaConfigurator> gamma_configurator_;
#endif  // defined(USE_AURA)

  float brightness_;
  bool screen_on_;

  std::unique_ptr<ColorTemperatureAnimation> color_temperature_animation_;
  std::unique_ptr<BrightnessAnimation> brightness_animation_;

  mojo::ReceiverSet<mojom::DisplaySettings> receivers_;
  mojo::RemoteSet<mojom::DisplaySettingsObserver> observers_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_DISPLAY_SETTINGS_MANAGER_IMPL_H_
