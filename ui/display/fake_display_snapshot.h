// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_FAKE_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_FAKE_DISPLAY_SNAPSHOT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/display/display_export.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/size.h"

namespace display {

// A display snapshot that doesn't correspond to a physical display, used when
// running off device.
class FakeDisplaySnapshot : public ui::DisplaySnapshot {
 public:
  // TODO(kylechar): Add a builder.

  FakeDisplaySnapshot(int64_t display_id,
                      const gfx::Size& display_size,
                      ui::DisplayConnectionType type,
                      std::string name);
  ~FakeDisplaySnapshot() override;

  // DisplaySnapshot:
  std::string ToString() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDisplaySnapshot);
};

}  // namespace display

#endif  // UI_DISPLAY_FAKE_DISPLAY_SNAPSHOT_H_
