// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_DISPLAY_UTIL_H_
#define UI_OZONE_COMMON_DISPLAY_UTIL_H_

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

class DisplayMode;
class DisplaySnapshot;

// Conforms to the std::UnaryPredicate interface such that it can be used to
// find a display with |display_id| in std:: containers (ie: std::vector).
class FindDisplayById {
 public:
  FindDisplayById(int64_t display_id);

  bool operator()(const DisplaySnapshot_Params& display) const;

 private:
  int64_t display_id_;
};

DisplayMode_Params GetDisplayModeParams(const DisplayMode& mode);
DisplaySnapshot_Params GetDisplaySnapshotParams(const DisplaySnapshot& display);

// Create a display using the Ozone command line parameters.
DisplaySnapshot_Params CreateSnapshotFromCommandLine();

}  // namespace ui

#endif  // UI_OZONE_COMMON_DISPLAY_UTIL_H_
