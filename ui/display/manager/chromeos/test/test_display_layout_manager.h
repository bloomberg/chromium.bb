// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/manager/chromeos/display_layout_manager.h"

namespace ui {
namespace test {

class TestDisplayLayoutManager : public DisplayLayoutManager {
 public:
  TestDisplayLayoutManager(ScopedVector<DisplaySnapshot> displays,
                           MultipleDisplayState display_state);
  ~TestDisplayLayoutManager() override;

  // DisplayLayoutManager:
  DisplayConfigurator::StateController* GetStateController() const override;
  DisplayConfigurator::SoftwareMirroringController*
  GetSoftwareMirroringController() const override;
  MultipleDisplayState GetDisplayState() const override;
  chromeos::DisplayPowerState GetPowerState() const override;
  bool GetDisplayLayout(const std::vector<DisplaySnapshot*>& displays,
                        MultipleDisplayState new_display_state,
                        chromeos::DisplayPowerState new_power_state,
                        std::vector<DisplayConfigureRequest>* requests,
                        gfx::Size* framebuffer_size) const override;
  std::vector<DisplaySnapshot*> GetDisplayStates() const override;
  bool IsMirroring() const override;

 private:
  ScopedVector<DisplaySnapshot> displays_;
  MultipleDisplayState display_state_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayLayoutManager);
};

}  // namespace test
}  // namespace ui

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_TEST_TEST_DISPLAY_LAYOUT_MANAGER_H_
