// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_DROP_TARGET_VIEW_H_
#define ASH_WM_OVERVIEW_DROP_TARGET_VIEW_H_

#include "ui/views/view.h"

namespace ash {
class RoundedRectView;

// DropTargetView represents a transparent view with border in overview. It
// includes a background view and plus icon. Dragged window in tablet mode can
// be dragged into it and then dropped into overview.
class DropTargetView : public views::View {
 public:
  explicit DropTargetView(bool has_plus_icon);
  DropTargetView(const DropTargetView&) = delete;
  DropTargetView& operator=(const DropTargetView&) = delete;
  ~DropTargetView() override = default;

  // Updates the visibility of |background_view_| since it is only shown when
  // drop target is selected in overview.
  void UpdateBackgroundVisibility(bool visible);

  // views::View:
  void Layout() override;

 private:
  class PlusIconView;

  RoundedRectView* background_view_ = nullptr;
  PlusIconView* plus_icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_DROP_TARGET_VIEW_H_
