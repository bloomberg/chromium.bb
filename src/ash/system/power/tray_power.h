// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TRAY_POWER_H_
#define ASH_SYSTEM_POWER_TRAY_POWER_H_

#include <memory>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {

namespace tray {

class PowerTrayView : public TrayItemView,
                      public PowerStatus::Observer,
                      public SessionObserver {
 public:
  explicit PowerTrayView(Shelf* shelf);

  ~PowerTrayView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // TrayItemView:
  void HandleLocaleChange() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  void UpdateStatus();
  void UpdateImage();

  std::u16string accessible_name_;
  std::u16string tooltip_;
  absl::optional<PowerStatus::BatteryImageInfo> info_;
  session_manager::SessionState icon_session_state_color_ =
      session_manager::SessionState::UNKNOWN;
  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TRAY_POWER_H_
