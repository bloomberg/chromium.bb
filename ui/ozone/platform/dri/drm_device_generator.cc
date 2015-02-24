// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/drm_device_generator.h"

#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

DrmDeviceGenerator::DrmDeviceGenerator() {
}

DrmDeviceGenerator::~DrmDeviceGenerator() {
}

scoped_refptr<DriWrapper> DrmDeviceGenerator::CreateDevice(
    const base::FilePath& device_path,
    base::File file) {
  scoped_refptr<DriWrapper> drm = new DriWrapper(device_path, file.Pass());
  if (drm->Initialize())
    return drm;

  return nullptr;
}

}  // namespace ui
