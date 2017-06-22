// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_CONTENT_VIEW_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_CONTENT_VIEW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/arc/notification/arc_notification_content_view_delegate.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/native/native_view_host.h"

namespace gfx {
class LinearAnimation;
}

namespace ui {
struct AXActionData;
}

namespace views {
class FocusTraversable;
class Widget;
}

namespace arc {

class ArcNotificationSurface;

// ArcNotificationContentView is a view to host NotificationSurface and show the
// content in itself. This is implemented as a child of ArcNotificationView.
class ArcNotificationContentView
    : public views::NativeViewHost,
      public views::ButtonListener,
      public aura::WindowObserver,
      public ArcNotificationItem::Observer,
      public ArcNotificationSurfaceManager::Observer,
      public gfx::AnimationDelegate {
 public:
  static const char kViewClassName[];

  explicit ArcNotificationContentView(ArcNotificationItem* item);
  ~ArcNotificationContentView() override;

  // views::View overrides:
  const char* GetClassName() const override;

  std::unique_ptr<ArcNotificationContentViewDelegate>
  CreateContentViewDelegate();

 private:
  friend class ArcNotificationContentViewTest;

  class ContentViewDelegate;
  class EventForwarder;
  class SettingsButton;
  class SlideHelper;

  // A image button class used for the settings button and the close button.
  // We can't use forward declaration for this class due to std::unique_ptr<>
  // requires size of this class.
  class ControlButton : public message_center::PaddedButton {
   public:
    explicit ControlButton(ArcNotificationContentView* owner);
    void OnFocus() override;
    void OnBlur() override;

   private:
    ArcNotificationContentView* const owner_;

    DISALLOW_COPY_AND_ASSIGN(ControlButton);
  };

  void CreateCloseButton();
  void CreateSettingsButton();
  void MaybeCreateFloatingControlButtons();
  void SetSurface(ArcNotificationSurface* surface);
  void UpdatePreferredSize();
  void UpdateControlButtonsVisibility();
  void UpdatePinnedState();
  void UpdateSnapshot();
  void AttachSurface();
  void ActivateToast();
  void StartControlButtonsColorAnimation();
  bool ShouldUpdateControlButtonsColor() const;
  void UpdateAccessibleName();

  // views::NativeViewHost
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  views::FocusTraversable* GetFocusTraversable() override;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // aura::WindowObserver
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ArcNotificationItem::Observer
  void OnItemDestroying() override;
  void OnItemUpdated() override;

  // ArcNotificationSurfaceManager::Observer:
  void OnNotificationSurfaceAdded(ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(ArcNotificationSurface* surface) override;

  // AnimationDelegate
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // If |item_| is null, we may be about to be destroyed. In this case,
  // we have to be careful about what we do.
  ArcNotificationItem* item_ = nullptr;
  ArcNotificationSurface* surface_ = nullptr;

  const std::string notification_key_;

  // A pre-target event handler to forward events on the surface to this view.
  // Using a pre-target event handler instead of a target handler on the surface
  // window because it has descendant aura::Window and the events on them need
  // to be handled as well.
  // TODO(xiyuan): Revisit after exo::Surface no longer has an aura::Window.
  std::unique_ptr<EventForwarder> event_forwarder_;

  // A helper to observe slide transform/animation and use surface layer copy
  // when a slide is in progress and restore the surface when it finishes.
  std::unique_ptr<SlideHelper> slide_helper_;

  // A control buttons on top of NotificationSurface. Needed because the
  // aura::Window of NotificationSurface is added after hosting widget's
  // RootView thus standard notification control buttons are always below
  // it.
  std::unique_ptr<views::Widget> floating_control_buttons_widget_;

  views::View* control_buttons_view_ = nullptr;
  std::unique_ptr<ControlButton> close_button_;
  ControlButton* settings_button_ = nullptr;

  // Protects from call loops between Layout and OnWindowBoundsChanged.
  bool in_layout_ = false;

  std::unique_ptr<gfx::LinearAnimation> control_button_color_animation_;

  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationContentView);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_CONTENT_VIEW_H_
