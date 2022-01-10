// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Point;
}

namespace views {
class BubbleBorder;
}

namespace autofill {

// Class that deals with the event handling for Autofill-style popups. This
// class should only be instantiated by sub-classes.
class AutofillPopupBaseView : public views::WidgetDelegateView,
                              public views::WidgetFocusChangeListener,
                              public views::WidgetObserver {
 public:
  METADATA_HEADER(AutofillPopupBaseView);

  // Consider the input element is |kElementBorderPadding| pixels larger at the
  // top and at the bottom in order to reposition the dropdown, so that it
  // doesn't look too close to the element.
  static const int kElementBorderPadding = 1;

  AutofillPopupBaseView(const AutofillPopupBaseView&) = delete;
  AutofillPopupBaseView& operator=(const AutofillPopupBaseView&) = delete;

  static int GetCornerRadius();

  // views::View:
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // Notify accessibility that an item has been selected.
  void NotifyAXSelection(View*);

  // Get colors used throughout various popup UIs, based on the current native
  // theme.
  SkColor GetBackgroundColor() const;
  SkColor GetForegroundColor() const;
  SkColor GetSelectedBackgroundColor() const;
  SkColor GetSelectedForegroundColor() const;
  SkColor GetFooterBackgroundColor() const;
  SkColor GetSeparatorColor() const;
  SkColor GetWarningColor() const;

 protected:
  AutofillPopupBaseView(base::WeakPtr<AutofillPopupViewDelegate> delegate,
                        views::Widget* parent_widget);
  ~AutofillPopupBaseView() override;

  // Show this popup. Idempotent. Returns |true| if popup is shown, |false|
  // otherwise.
  bool DoShow();

  // Hide the widget and delete |this|.
  void DoHide();

  // Ensure the child views are not rendered beyond the bubble border
  // boundaries. Should be overridden together with CreateBorder.
  void UpdateClipPath();

  // Returns the bounds of the containing browser window in screen space.
  gfx::Rect GetTopWindowBounds() const;

  // Returns the bounds of the content area in screen space.
  gfx::Rect GetContentAreaBounds() const;

  // Update size of popup and paint. If there is insufficient height to draw the
  // popup, it hides and thus deletes |this| and returns false. (virtual for
  // testing).
  virtual bool DoUpdateBoundsAndRedrawPopup();

  // Returns the border to be applied to the popup.
  virtual std::unique_ptr<views::Border> CreateBorder();

  // Returns the optimal bounds to place the bubble with |preferred_size| and
  // places an arrow on the bubble border to point towards |element_bounds|
  // within |max_bounds_for_popup|.
  gfx::Rect GetOptionalPositionAndPlaceArrowOnBubble(
      const gfx::Rect& element_bounds,
      const gfx::Rect& max_bounds_for_popup,
      const gfx::Size& preferred_size);

 private:
  friend class AutofillPopupBaseViewTest;

  // views::Views implementation.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::WidgetFocusChangeListener implementation.
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // views::WidgetObserver implementation.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Stop observing the widget.
  void RemoveWidgetObservers();

  // Hide the controller of this view. This assumes that doing so will
  // eventually hide this view in the process.
  void HideController(PopupHidingReason reason);

  // Must return the container view for this popup.
  gfx::NativeView container_view();

  // Controller for this popup. Weak reference.
  base::WeakPtr<AutofillPopupViewDelegate> delegate_;

  // The widget of the window that triggered this popup. Weak reference.
  raw_ptr<views::Widget> parent_widget_;

  // The time when the popup was shown.
  base::Time show_time_;

  // Ensures that the menu start event is not fired redundantly.
  bool is_ax_menu_start_event_fired_ = false;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_
