// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_wrapper.h"

#include <gbm.h>

namespace ui {

GbmWrapper::GbmWrapper(const char* device_path, bool use_sync_flips)
    : DriWrapper(device_path, use_sync_flips), device_(nullptr) {
}

GbmWrapper::~GbmWrapper() {
  if (device_)
    gbm_device_destroy(device_);
}

void GbmWrapper::Initialize() {
  DriWrapper::Initialize();
  device_ = gbm_create_device(get_fd());
  if (!device_)
    LOG(FATAL) << "Unable to initialize GBM";
}

}  // namespace ui
