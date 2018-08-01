// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/linux/gbm_device.h"

#include <gbm.h>

namespace ui {

GbmDevice::GbmDevice() {}

GbmDevice::~GbmDevice() {
  if (device_) {
    gbm_device_destroy(device_);
    device_ = nullptr;
  }
}

bool GbmDevice::Initialize(int fd) {
  DCHECK(!device_);
  device_ = gbm_create_device(fd);
  return !!device_;
}

}  // namespace ui
