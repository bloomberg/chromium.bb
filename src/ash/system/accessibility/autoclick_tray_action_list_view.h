// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_ACTION_LIST_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_ACTION_LIST_VIEW_H_

#include "ash/public/interfaces/accessibility_controller_enums.mojom.h"
#include "ash/system/tray/system_menu_button.h"
#include "base/macros.h"

namespace ash {
class AutoclickTray;

// View for the list of Autoclick action options.
class AutoclickTrayActionListView : public views::View {
 public:
  AutoclickTrayActionListView(AutoclickTray* tray,
                              mojom::AutoclickEventType selected_event_type);
  ~AutoclickTrayActionListView() override = default;

  void PerformTrayActionViewAction(mojom::AutoclickEventType type);

 private:
  AutoclickTray* autoclick_tray_;
  mojom::AutoclickEventType selected_event_type_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTrayActionListView);
};

}  // namespace ash

#endif  // defined ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_TRAY_ACTION_LIST_VIEW_H_
