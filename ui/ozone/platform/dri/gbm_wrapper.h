// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_WRAPPER_H_
#define UI_OZONE_PLATFORM_DRI_GBM_WRAPPER_H_

#include "ui/ozone/platform/dri/dri_wrapper.h"

struct gbm_device;

namespace ui {

class GbmWrapper : public DriWrapper {
 public:
  GbmWrapper(const base::FilePath& device_path);
  GbmWrapper(const base::FilePath& device_path, base::File file);

  gbm_device* device() const { return device_; }

  // DriWrapper implementation:
  bool Initialize() override;

 private:
  ~GbmWrapper() override;

  gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(GbmWrapper);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_WRAPPER_H_
