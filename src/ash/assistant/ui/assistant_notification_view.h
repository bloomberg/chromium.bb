// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class BorderShadowLayerDelegate;
class Label;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// AssistantNotificationView is the view for a mojom::AssistantNotification
// which appears in Assistant UI. Its parent is AssistantNotificationOverlay.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantNotificationView
    : public views::View,
      public views::ButtonListener,
      public ui::ImplicitAnimationObserver,
      public AssistantNotificationModelObserver {
 public:
  AssistantNotificationView(AssistantViewDelegate* delegate,
                            const AssistantNotification* notification);
  ~AssistantNotificationView() override;

  // views::View:
  const char* GetClassName() const override;
  void AddedToWidget() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // AssistantNotificationModelObserver:
  void OnNotificationUpdated(
      const AssistantNotification* notification) override;
  void OnNotificationRemoved(const AssistantNotification* notification,
                             bool from_server) override;

 private:
  void InitLayout(const AssistantNotification* notification);
  void UpdateBackground();
  void UpdateVisibility(bool visible);

  AssistantViewDelegate* const delegate_;  // Owned by AssistantController.
  const std::string notification_id_;

  views::View* container_;  // Owned by view hierarchy.
  views::Label* title_;     // Owned by view hierarchy.
  views::Label* message_;   // Owned by view hierarchy.

  std::vector<views::View*> buttons_;  // Owned by view hierarchy.

  // Background/shadow.
  ui::Layer background_layer_;
  std::unique_ptr<views::BorderShadowLayerDelegate> shadow_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_VIEW_H_
