// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#define UI_UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#pragma once

#include "base/timer.h"
#include "ui/gfx/point.h"
#include "ui/views/touchui/touch_selection_controller.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// Touch specific implementation of TouchSelectionController. Responsible for
// displaying selection handles and menu elements relevant in a touch interface.
class VIEWS_EXPORT TouchSelectionControllerImpl
    : public TouchSelectionController {
 public:
  // Use TextSelectionController::create().
  explicit TouchSelectionControllerImpl(TouchSelectionClientView* client_view);

  virtual ~TouchSelectionControllerImpl();

  // TextSelectionController.
  virtual void SelectionChanged(const gfx::Point& p1,
                                const gfx::Point& p2) OVERRIDE;

  virtual void ClientViewLostFocus() OVERRIDE;

 private:
  friend class TouchSelectionControllerImplTest;
  class SelectionHandleView;
  class TouchContextMenuView;

  // Callback to inform the client view that the selection handle has been
  // dragged, hence selection may need to be updated.
  void SelectionHandleDragged(const gfx::Point& drag_pos);

  // Convenience method to convert a point from a selection handle's coordinate
  // system to that of the client view.
  void ConvertPointToClientView(SelectionHandleView* source, gfx::Point* point);

  // Checks if the client view supports a context menu command.
  bool IsCommandIdEnabled(int command_id) const;

  // Sends a context menu command to the client view.
  void ExecuteCommand(int command_id);

  // Time to show context menu.
  void ContextMenuTimerFired();

  // Convenience method to update the position/visibility of the context menu.
  void UpdateContextMenu(const gfx::Point& p1, const gfx::Point& p2);

  // Convenience method for hiding context menu.
  void HideContextMenu();

  // Convenience methods for testing.
  gfx::Point GetSelectionHandle1Position();
  gfx::Point GetSelectionHandle2Position();
  bool IsSelectionHandle1Visible();
  bool IsSelectionHandle2Visible();

  TouchSelectionClientView* client_view_;
  scoped_ptr<SelectionHandleView> selection_handle_1_;
  scoped_ptr<SelectionHandleView> selection_handle_2_;
  scoped_ptr<TouchContextMenuView> context_menu_;

  // Timer to trigger |context_menu| (|context_menu| is not shown if the
  // selection handles are being updated. It appears only when the handles are
  // stationary for a certain amount of time).
  base::OneShotTimer<TouchSelectionControllerImpl> context_menu_timer_;

  // Pointer to the SelectionHandleView being dragged during a drag session.
  SelectionHandleView* dragging_handle_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerImpl);
};

}  // namespace views

#endif  // UI_UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
