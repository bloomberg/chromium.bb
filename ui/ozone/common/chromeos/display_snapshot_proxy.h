// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_CHROMEOS_DISPLAY_SNAPSHOT_PROXY_H_
#define UI_OZONE_COMMON_CHROMEOS_DISPLAY_SNAPSHOT_PROXY_H_

#include "ui/display/types/display_snapshot.h"

namespace ui {

struct DisplaySnapshot_Params;

class DisplaySnapshotProxy : public DisplaySnapshot {
 public:
  DisplaySnapshotProxy(const DisplaySnapshot_Params& params);
  virtual ~DisplaySnapshotProxy();

  // DisplaySnapshot override:
  virtual std::string ToString() const OVERRIDE;

 private:
  std::string string_representation_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshotProxy);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_CHROMEOS_DISPLAY_SNAPSHOT_PROXY_H_
