// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_GBM_WRAPPER_H_
#define UI_OZONE_COMMON_LINUX_GBM_WRAPPER_H_

#include <memory>

#include "ui/ozone/common/linux/gbm_device.h"

namespace ui {

std::unique_ptr<ui::GbmDevice> CreateGbmDevice(int fd);

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_GBM_WRAPPER_H_
