// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_TEST_TEST_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_CHROMEOS_TEST_TEST_DISPLAY_SNAPSHOT_H_

#include "ui/display/display_export.h"
#include "ui/display/types/display_snapshot.h"

namespace ui {

class DISPLAY_EXPORT TestDisplaySnapshot : public DisplaySnapshot {
 public:
  TestDisplaySnapshot();
  TestDisplaySnapshot(int64_t display_id,
                      const gfx::Point& origin,
                      const gfx::Size& physical_size,
                      DisplayConnectionType type,
                      bool is_aspect_preserving_scaling,
                      int64_t product_id,
                      const std::vector<const DisplayMode*>& modes,
                      const DisplayMode* current_mode,
                      const DisplayMode* native_mode);
  ~TestDisplaySnapshot() override;

  void set_type(DisplayConnectionType type) { type_ = type; }
  void set_modes(const std::vector<const DisplayMode*>& modes) {
    modes_ = modes;
  }
  void set_current_mode(const ui::DisplayMode* mode) { current_mode_ = mode; }
  void set_native_mode(const ui::DisplayMode* mode) { native_mode_ = mode; }
  void set_is_aspect_preserving_scaling(bool state) {
    is_aspect_preserving_scaling_ = state;
  }
  void set_display_id(int64_t id) { display_id_ = id; }

  // DisplaySnapshot overrides:
  std::string ToString() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDisplaySnapshot);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_TEST_TEST_DISPLAY_SNAPSHOT_H_
