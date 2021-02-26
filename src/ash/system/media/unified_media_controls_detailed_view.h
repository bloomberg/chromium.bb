// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_H_
#define ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_H_

#include "ash/system/tray/tray_detailed_view.h"

namespace ash {

// Detailed view displaying all active media sessions
class UnifiedMediaControlsDetailedView : public TrayDetailedView {
 public:
  UnifiedMediaControlsDetailedView(
      DetailedViewDelegate* delegate,
      std::unique_ptr<views::View> notification_list_view);
  ~UnifiedMediaControlsDetailedView() override = default;

 private:
  // TrayDetailedView implementation.
  void CreateExtraTitleRowButtons() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_DETAILED_VIEW_H_
