// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ui {
class Event;
}

namespace gfx {
class LinearAnimation;
}

namespace message_center {

class MessageView;
class PaddedButton;

class MESSAGE_CENTER_EXPORT NotificationControlButtonsView
    : public views::View,
      public views::ButtonListener,
      public gfx::AnimationDelegate {
 public:
  // String to be returned by GetClassName() method.
  static const char kViewClassName[];

  explicit NotificationControlButtonsView(MessageView* message_view);
  ~NotificationControlButtonsView() override;

  // Change the visibility of the close button. True to show, false to hide.
  void ShowCloseButton(bool show);
  // Change the visibility of the settings button. True to show, false to hide.
  void ShowSettingsButton(bool show);

  // Set the background color of the view.
  void SetBackgroundColor(const SkColor& target_bgcolor);

  // Set and get the visi8blity of buttons.
  void SetButtonsVisible(bool visible);
  bool GetButtonsVisible() const;

  // Request the focus on the close button.
  void RequestFocusOnCloseButton();

  // Return the focus status of the close button. True if the focus is on the
  // close button, false otherwise.
  bool IsCloseButtonFocused() const;
  // Return the focus status of the settings button. True if the focus is on the
  // close button, false otherwise.
  bool IsSettingsButtonFocused() const;

  // Methods for testing.
  message_center::PaddedButton* close_button_for_testing() const;
  message_center::PaddedButton* settings_button_for_testing() const;

#if defined(OS_CHROMEOS)
  void ReparentToWidgetLayer();
  void AdjustLayerBounds();
#endif  // defined(OS_CHROMEOS)

  // views::View
  const char* GetClassName() const override;
#if defined(OS_CHROMEOS)
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  gfx::Vector2d CalculateOffsetToAncestorWithLayer(
      ui::Layer** layer_parent) override;
#endif  // defined(OS_CHROMEOS)

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // gfx::AnimationDelegate
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  // InnerView is the container for buttons. NotificationControlButtonsView has
  // only one child which is InnerView.
  class InnerView;

  MessageView* message_view_;

  std::unique_ptr<InnerView> buttons_container_;
  std::unique_ptr<message_center::PaddedButton> close_button_;
  std::unique_ptr<message_center::PaddedButton> settings_button_;

  std::unique_ptr<gfx::LinearAnimation> bgcolor_animation_;
  SkColor bgcolor_origin_;
  SkColor bgcolor_target_;

#if defined(OS_CHROMEOS)
  bool is_layer_parent_widget_ = false;
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(NotificationControlButtonsView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_
