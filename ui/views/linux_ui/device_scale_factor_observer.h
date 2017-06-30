// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_DEVICE_SCALE_FACTOR_OBSERVER_H_
#define UI_VIEWS_LINUX_UI_DEVICE_SCALE_FACTOR_OBSERVER_H_

namespace views {

class DeviceScaleFactorObserver {
 public:
  virtual ~DeviceScaleFactorObserver() {}

  virtual void OnDeviceScaleFactorChanged() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_LINUX_UI_DEVICE_SCALE_FACTOR_OBSERVER_H_
