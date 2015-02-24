// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_wrapper.h"

#include <gbm.h>

namespace ui {

GbmWrapper::GbmWrapper(const base::FilePath& device_path)
    : DriWrapper(device_path), device_(nullptr) {
}

GbmWrapper::GbmWrapper(const base::FilePath& device_path, base::File file)
    : DriWrapper(device_path, file.Pass()), device_(nullptr) {
}

GbmWrapper::~GbmWrapper() {
  if (device_)
    gbm_device_destroy(device_);
}

bool GbmWrapper::Initialize() {
  if (!DriWrapper::Initialize())
    return false;

  device_ = gbm_create_device(get_fd());
  if (!device_) {
    LOG(ERROR) << "Unable to initialize GBM";
    return false;
  }

  return true;
}

}  // namespace ui
