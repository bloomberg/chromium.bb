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
#include "ui/views/controls/slide_out_view.h"

namespace views {
class Painter;
class ScrollView;
}

namespace message_center {

class MessageCenterController;

// Individual notifications constants.
const int kPaddingBetweenItems = 10;
const int kPaddingHorizontal = 18;
const int kWebNotificationButtonWidth = 32;
const int kWebNotificationIconSize = 40;

// An base class for a notification entry. Contains background and other
// elements shared by derived notification views.
class MESSAGE_CENTER_EXPORT MessageView : public views::SlideOutView {
 public:
  MessageView(MessageCenterController* controller,
              const Notification& notification);
  ~MessageView() override;

  // Updates this view with the new data contained in the notification.
  virtual void UpdateWithNotification(const Notification& notification);

  // Returns the insets for the shadow it will have for rich notification.
  static gfx::Insets GetShadowInsets();

  // Creates a shadow around the notification.
  void CreateShadowBorder();

  virtual bool IsCloseButtonFocused() const = 0;
  virtual void RequestFocusOnCloseButton() = 0;
  virtual bool IsPinned() const = 0;
  virtual void UpdateControlButtonsVisibility() = 0;

  void OnCloseButtonPressed();

  // Overridden from views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void Layout() override;

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }
  std::string notification_id() { return notification_id_; }
  NotifierId notifier_id() { return notifier_id_; }
  const base::string16& display_source() const { return display_source_; }

  void set_controller(MessageCenterController* controller) {
    controller_ = controller;
  }

 protected:
  // Overridden from views::SlideOutView:
  void OnSlideOut() override;

  // Creates and add close button to view hierarchy when necessary. Derived
  // classes should call this after its view hierarchy is populated to ensure
  // it is on top of other views.
  void CreateOrUpdateCloseButtonView(const Notification& notification);

  // Changes the background color being used by |background_view_| and schedules
  // a paint.
  virtual void SetDrawBackgroundAsActive(bool active);

  views::View* background_view() { return background_view_; }
  views::ScrollView* scroller() { return scroller_; }
  MessageCenterController* controller() { return controller_; }

 private:
  MessageCenterController* controller_;  // Weak, lives longer then views.
  std::string notification_id_;
  NotifierId notifier_id_;
  views::View* background_view_ = nullptr;  // Owned by views hierarchy.
  views::ScrollView* scroller_ = nullptr;

  base::string16 accessible_name_;

  base::string16 display_source_;

  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
