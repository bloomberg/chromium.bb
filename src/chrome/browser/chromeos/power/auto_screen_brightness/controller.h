// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

class Adapter;
class AlsReaderImpl;
class BrightnessMonitorImpl;
class MetricsReporter;
class ModelConfigLoaderImpl;
class ModellerImpl;

// This controller class sets up and destroys all components needed for the auto
// screen brightness feature.
class Controller {
 public:
  Controller();
  ~Controller();

 private:
  std::unique_ptr<MetricsReporter> metrics_reporter_;
  std::unique_ptr<AlsReaderImpl> als_reader_;
  std::unique_ptr<BrightnessMonitorImpl> brightness_monitor_;
  std::unique_ptr<ModelConfigLoaderImpl> model_config_loader_;
  std::unique_ptr<ModellerImpl> modeller_;
  std::unique_ptr<Adapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_CONTROLLER_H_
