// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

DisplayMode_Params::DisplayMode_Params()
    : size(), is_interlaced(false), refresh_rate(0.0f) {}

DisplayMode_Params::~DisplayMode_Params() {}

DisplaySnapshot_Params::DisplaySnapshot_Params()
    : display_id(0),
      has_proper_display_id(false),
      origin(),
      physical_size(),
      type(ui::DISPLAY_CONNECTION_TYPE_NONE),
      is_aspect_preserving_scaling(false),
      has_overscan(false),
      display_name(),
      modes(),
      has_current_mode(false),
      current_mode(),
      has_native_mode(false),
      native_mode() {}

DisplaySnapshot_Params::~DisplaySnapshot_Params() {}

}  // namespace ui
