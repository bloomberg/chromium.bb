// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/chromeos/display_mode_proxy.h"

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

DisplayModeProxy::DisplayModeProxy(const DisplayMode_Params& params)
    : DisplayMode(params.size, params.is_interlaced, params.refresh_rate) {}

DisplayModeProxy::~DisplayModeProxy() {}

}  // namespace ui
