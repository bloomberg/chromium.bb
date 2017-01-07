// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_SNAPSHOT_VIRTUAL_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_SNAPSHOT_VIRTUAL_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"

namespace display {

// This class represents a virtual display to be enabled on demand. The display
// is constructed for the desired pixel resolution.
class DISPLAY_MANAGER_EXPORT DisplaySnapshotVirtual : public DisplaySnapshot {
 public:
  // |display_id| is the numerical identifier for the virtual display,
  // |display_size| is the pixel dimensions for the display.
  DisplaySnapshotVirtual(int64_t display_id, const gfx::Size& display_size);
  ~DisplaySnapshotVirtual() override;

  // DisplaySnapshot overrides.
  std::string ToString() const override;

 private:
  std::unique_ptr<DisplayMode> mode_;
  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshotVirtual);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_SNAPSHOT_VIRTUAL_H_
