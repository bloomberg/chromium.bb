// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_

#include "base/logging.h"

namespace device {

class PlatformSensorConfiguration {
 public:
  PlatformSensorConfiguration();
  explicit PlatformSensorConfiguration(double frequency);
  ~PlatformSensorConfiguration();

  bool operator==(const PlatformSensorConfiguration& other) const;

  // Platform dependent implementations can override this operator if different
  // optimal configuration comparison is required. By default, only frequency is
  // used to compare two configurations.
  virtual bool operator>(const PlatformSensorConfiguration& other) const;

  void set_frequency(double frequency);
  double frequency() const { return frequency_; }

  void set_suppress_on_change_events(bool suppress_on_change_events) {
    suppress_on_change_events_ = suppress_on_change_events;
  }
  bool suppress_on_change_events() const { return suppress_on_change_events_; }

 private:
  double frequency_ = 1.0;  // 1 Hz by default.
  bool suppress_on_change_events_ = false;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_PLATFORM_SENSOR_CONFIGURATION_H_
