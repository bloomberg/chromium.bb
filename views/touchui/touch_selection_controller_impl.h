// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#define VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#pragma once

#include "ui/gfx/point.h"
#include "views/touchui/touch_selection_controller.h"
#include "views/view.h"
#include "views/views_export.h"

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

  // Callback to inform the client view that the selection handle has been
  // dragged, hence selection may need to be updated.
  void SelectionHandleDragged(const gfx::Point& drag_pos);

  // Convenience method to convert a point from a selection handle's coordinate
  // system to that of the client view.
  void ConvertPointToClientView(SelectionHandleView* source, gfx::Point* point);

  // Convenience methods for testing.
  gfx::Point GetSelectionHandle1Position();
  gfx::Point GetSelectionHandle2Position();
  bool IsSelectionHandle1Visible();
  bool IsSelectionHandle2Visible();

  TouchSelectionClientView* client_view_;
  scoped_ptr<SelectionHandleView> selection_handle_1_;
  scoped_ptr<SelectionHandleView> selection_handle_2_;

  // Pointer to the SelectionHandleView being dragged during a drag session.
  SelectionHandleView* dragging_handle_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerImpl);
};

}  // namespace views

#endif  // VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
