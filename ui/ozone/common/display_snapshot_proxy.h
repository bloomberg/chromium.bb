// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_DISPLAY_SNAPSHOT_PROXY_H_
#define UI_OZONE_COMMON_DISPLAY_SNAPSHOT_PROXY_H_

#include "base/macros.h"
#include "ui/display/types/display_snapshot_mojo.h"

namespace ui {

struct DisplaySnapshot_Params;

class DisplaySnapshotProxy : public display::DisplaySnapshotMojo {
 public:
  DisplaySnapshotProxy(const DisplaySnapshot_Params& params);
  ~DisplaySnapshotProxy() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshotProxy);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_DISPLAY_SNAPSHOT_PROXY_H_
