// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/views/slide_out_controller.h"
#include "ui/views/view.h"

namespace views {
class Painter;
class ScrollView;
}

namespace message_center {

class Notification;
class NotificationControlButtonsView;
class MessageViewDelegate;

// An base class for a notification entry. Contains background and other
// elements shared by derived notification views.
class MESSAGE_CENTER_EXPORT MessageView
    : public views::View,
      public views::SlideOutController::Delegate {
 public:
  MessageView(MessageViewDelegate* delegate, const Notification& notification);
  ~MessageView() override;

  // Updates this view with the new data contained in the notification.
  virtual void UpdateWithNotification(const Notification& notification);

  // Creates a shadow around the notification and changes slide-out behavior.
  void SetIsNested();

  virtual NotificationControlButtonsView* GetControlButtonsView() const = 0;
  virtual bool IsCloseButtonFocused() const = 0;
  virtual void RequestFocusOnCloseButton() = 0;
  virtual void UpdateControlButtonsVisibility() = 0;

  virtual void SetExpanded(bool expanded);
  virtual bool IsExpanded() const;

  void OnCloseButtonPressed();
  void OnSettingsButtonPressed();

  // views::View
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void Layout() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::SlideOutController::Delegate
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideChanged() override;
  void OnSlideOut() override;

  bool GetPinned() const;

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }
  std::string notification_id() const { return notification_id_; }
  void set_delegate(MessageViewDelegate* delegate) { delegate_ = delegate; }

#if defined(OS_CHROMEOS)
  // By calling this, all notifications are treated as non-pinned forcibly.
  void set_force_disable_pinned() { force_disable_pinned_ = true; }
#endif

 protected:
  // Creates and add close button to view hierarchy when necessary. Derived
  // classes should call this after its view hierarchy is populated to ensure
  // it is on top of other views.
  void CreateOrUpdateCloseButtonView(const Notification& notification);

  // Changes the background color being used by |background_view_| and schedules
  // a paint.
  virtual void SetDrawBackgroundAsActive(bool active);

  views::View* background_view() { return background_view_; }
  views::ScrollView* scroller() { return scroller_; }
  MessageViewDelegate* delegate() { return delegate_; }

 private:
  MessageViewDelegate* delegate_;
  std::string notification_id_;
  views::View* background_view_ = nullptr;  // Owned by views hierarchy.
  views::ScrollView* scroller_ = nullptr;

  base::string16 accessible_name_;

  // Flag if the notification is set to pinned or not.
  bool pinned_ = false;
  // Flag if pin is forcibly disabled on this view. If true, the view is never
  // pinned regardless of the value of |pinned_|.
  bool force_disable_pinned_ = false;

  std::unique_ptr<views::Painter> focus_painter_;

  views::SlideOutController slide_out_controller_;

  // True if |this| is embedded in another view. Equivalent to |!top_level| in
  // MessageViewFactory parlance.
  bool is_nested_ = false;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
