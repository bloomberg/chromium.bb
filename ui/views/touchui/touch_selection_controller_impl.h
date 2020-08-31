// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WidgetDelegateView;

// Touch specific implementation of TouchEditingControllerDeprecated.
// Responsible for displaying selection handles and menu elements relevant in a
// touch interface.
class VIEWS_EXPORT TouchSelectionControllerImpl
    : public ui::TouchEditingControllerDeprecated,
      public ui::TouchSelectionMenuClient,
      public WidgetObserver,
      public ui::EventObserver {
 public:
  class EditingHandleView;

  // Use ui::TouchEditingControllerFactory::Create() instead.
  explicit TouchSelectionControllerImpl(ui::TouchEditable* client_view);
  ~TouchSelectionControllerImpl() override;

  // ui::TouchEditingControllerDeprecated:
  void SelectionChanged() override;
  bool IsHandleDragInProgress() override;
  void HideHandles(bool quick) override;

  void ShowQuickMenuImmediatelyForTesting();

 private:
  friend class TouchSelectionControllerImplTest;

  void SetDraggingHandle(EditingHandleView* handle);

  // Callback to inform the client view that the selection handle has been
  // dragged, hence selection may need to be updated. |drag_pos| is the new
  // position for the edge of the selection corresponding to |dragging_handle_|,
  // specified in handle's coordinates
  void SelectionHandleDragged(const gfx::Point& drag_pos);

  // Convenience method to convert a point from a selection handle's coordinate
  // system to that of the client view.
  void ConvertPointToClientView(EditingHandleView* source, gfx::Point* point);

  // Convenience method to set a handle's selection bound and hide it if it is
  // located out of client view.
  void SetHandleBound(EditingHandleView* handle,
                      const gfx::SelectionBound& bound,
                      const gfx::SelectionBound& bound_in_screen);

  // Checks if handle should be shown for selection bound.
  // |bound| should be the clipped version of the selection bound.
  bool ShouldShowHandleFor(const gfx::SelectionBound& bound) const;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  base::string16 GetSelectedText() override;

  // WidgetObserver:
  void OnWidgetClosing(Widget* widget) override;
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // Time to show quick menu.
  void QuickMenuTimerFired();

  void StartQuickMenuTimer();

  // Convenience method to update the position/visibility of the quick menu.
  void UpdateQuickMenu();

  // Convenience method for hiding quick menu.
  void HideQuickMenu();

  // Convenience method to calculate anchor rect for quick menu, in screen
  // coordinates.
  gfx::Rect GetQuickMenuAnchorRect() const;

  // Convenience methods for testing.
  gfx::NativeView GetCursorHandleNativeView();
  gfx::SelectionBound::Type GetSelectionHandle1Type();
  gfx::Rect GetSelectionHandle1Bounds();
  gfx::Rect GetSelectionHandle2Bounds();
  gfx::Rect GetCursorHandleBounds();
  bool IsSelectionHandle1Visible();
  bool IsSelectionHandle2Visible();
  bool IsCursorHandleVisible();
  gfx::Rect GetExpectedHandleBounds(const gfx::SelectionBound& bound);
  WidgetDelegateView* GetHandle1View();
  WidgetDelegateView* GetHandle2View();

  ui::TouchEditable* client_view_;
  Widget* client_widget_ = nullptr;
  std::unique_ptr<EditingHandleView> selection_handle_1_;
  std::unique_ptr<EditingHandleView> selection_handle_2_;
  std::unique_ptr<EditingHandleView> cursor_handle_;
  bool command_executed_ = false;
  base::TimeTicks selection_start_time_;

  // Timer to trigger quick menu (Quick menu is not shown if the selection
  // handles are being updated. It appears only when the handles are stationary
  // for a certain amount of time).
  base::OneShotTimer quick_menu_timer_;

  // Pointer to the SelectionHandleView being dragged during a drag session.
  EditingHandleView* dragging_handle_ = nullptr;

  // In cursor mode, the two selection bounds are the same and correspond to
  // |cursor_handle_|; otherwise, they correspond to |selection_handle_1_| and
  // |selection_handle_2_|, respectively. These values should be used when
  // selection bounds needed rather than position of handles which might be
  // invalid when handles are hidden.
  gfx::SelectionBound selection_bound_1_;
  gfx::SelectionBound selection_bound_2_;

  // Selection bounds, clipped to client view's boundaries.
  gfx::SelectionBound selection_bound_1_clipped_;
  gfx::SelectionBound selection_bound_2_clipped_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerImpl);
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
