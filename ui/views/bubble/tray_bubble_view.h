// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_TRAY_BUBBLE_VIEW_H_
#define UI_VIEWS_BUBBLE_TRAY_BUBBLE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/views_export.h"

namespace views {
class BoxLayout;
class View;
class Widget;
}

namespace views {

namespace internal {
class TrayBubbleContentMask;
}

// Specialized bubble view for bubbles associated with a tray icon (e.g. the
// Ash status area). Mostly this handles custom anchor location and arrow and
// border rendering. This also has its own delegate for handling mouse events
// and other implementation specific details.
class VIEWS_EXPORT TrayBubbleView : public BubbleDialogDelegateView,
                                    public MouseWatcherListener {
 public:
  // AnchorAlignment determines to which side of the anchor the bubble will
  // align itself.
  enum AnchorAlignment {
    ANCHOR_ALIGNMENT_BOTTOM,
    ANCHOR_ALIGNMENT_LEFT,
    ANCHOR_ALIGNMENT_RIGHT,
  };

  class VIEWS_EXPORT Delegate {
   public:
    typedef TrayBubbleView::AnchorAlignment AnchorAlignment;

    Delegate() {}
    virtual ~Delegate() {}

    // Called when the view is destroyed. Any pointers to the view should be
    // cleared when this gets called.
    virtual void BubbleViewDestroyed() = 0;

    // Called when the mouse enters/exits the view.
    // Note: This event will only be called if the mouse gets actively moved by
    // the user to enter the view.
    virtual void OnMouseEnteredView() = 0;
    virtual void OnMouseExitedView() = 0;

    // Called to register/unregister accelerators for TrayBubbleView.
    // TrayBubbleView wants to register those accelerators at the global level.
    // Those accelerators are used to activate TrayBubbleView, i.e. those
    // accelerators need to be processed even if TrayBubbleView is not active.
    // UnregisterAllAccelerators can be called even if RegisterAccelerators is
    // not called.
    virtual void RegisterAccelerators(
        const std::vector<ui::Accelerator>& accelerators,
        TrayBubbleView* tray_bubble_view) = 0;
    virtual void UnregisterAllAccelerators(
        TrayBubbleView* tray_bubble_view) = 0;

    // Called from GetAccessibleNodeData(); should return the appropriate
    // accessible name for the bubble.
    virtual base::string16 GetAccessibleNameForBubble() = 0;

    // Should return true if extra keyboard accessibility is enabled.
    // TrayBubbleView will put focus on the default item if extra keyboard
    // accessibility is enabled.
    virtual bool ShouldEnableExtraKeyboardAccessibility() = 0;

    // Called when a bubble wants to hide/destroy itself (e.g. last visible
    // child view was closed).
    virtual void HideBubble(const TrayBubbleView* bubble_view) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  struct VIEWS_EXPORT InitParams {
    InitParams();
    InitParams(const InitParams& other);
    Delegate* delegate = nullptr;
    gfx::NativeWindow parent_window = nullptr;
    View* anchor_view = nullptr;
    AnchorAlignment anchor_alignment = ANCHOR_ALIGNMENT_BOTTOM;
    int min_width = 0;
    int max_width = 0;
    int max_height = 0;
    bool close_on_deactivate = true;
    // If not provided, the bg color will be derived from the NativeTheme.
    base::Optional<SkColor> bg_color;
  };

  explicit TrayBubbleView(const InitParams& init_params);
  ~TrayBubbleView() override;

  // Returns whether a tray bubble is active.
  static bool IsATrayBubbleOpen();

  // Sets up animations, and show the bubble. Must occur after CreateBubble()
  // is called.
  void InitializeAndShowBubble();

  // Called whenever the bubble size or location may have changed.
  void UpdateBubble();

  // Sets the maximum bubble height and resizes the bubble.
  void SetMaxHeight(int height);

  // Sets the bottom padding that child views will be laid out within.
  void SetBottomPadding(int padding);

  // Sets the bubble width.
  void SetWidth(int width);

  // Returns the border insets. Called by TrayEventFilter.
  gfx::Insets GetBorderInsets() const;

  // Called when the delegate is destroyed. This must be called before the
  // delegate is actually destroyed. TrayBubbleView will do clean up in
  // ResetDelegate.
  void ResetDelegate();

  Delegate* delegate() { return delegate_; }

  void set_gesture_dragging(bool dragging) { is_gesture_dragging_ = dragging; }
  bool is_gesture_dragging() const { return is_gesture_dragging_; }

  // Overridden from views::WidgetDelegate.
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;
  base::string16 GetAccessibleWindowTitle() const override;

  // Overridden from views::BubbleDialogDelegateView.
  void OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                Widget* bubble_widget) const override;
  void OnWidgetClosing(Widget* widget) override;

  // Overridden from views::View.
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMaximumSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from MouseWatcherListener
  void MouseMovedOutOfHost() override;

  // Overridden from ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 protected:
  // Overridden from views::BubbleDialogDelegateView.
  int GetDialogButtons() const override;
  void SizeToContents() override;

  // Overridden from views::View.
  void ChildPreferredSizeChanged(View* child) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

 private:
  void CloseBubbleView();
  void ActivateAndStartNavigation(const ui::KeyEvent& key_event);

  // Focus the default item if no item is focused.
  void FocusDefaultIfNeeded();

  InitParams params_;
  BoxLayout* layout_;
  Delegate* delegate_;
  int preferred_width_;
  // |bubble_border_| and |owned_bubble_border_| point to the same thing, but
  // the latter ensures we don't leak it before passing off ownership.
  BubbleBorder* bubble_border_;
  std::unique_ptr<views::BubbleBorder> owned_bubble_border_;
  std::unique_ptr<internal::TrayBubbleContentMask> bubble_content_mask_;
  bool is_gesture_dragging_;

  // True once the mouse cursor was actively moved by the user over the bubble.
  // Only then the OnMouseExitedView() event will get passed on to listeners.
  bool mouse_actively_entered_;

  // Used to find any mouse movements.
  std::unique_ptr<MouseWatcher> mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleView);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_TRAY_BUBBLE_VIEW_H_
