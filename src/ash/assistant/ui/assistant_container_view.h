// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_

#include <memory>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

class AssistantController;
class AssistantMainView;
class AssistantMiniView;
class AssistantWebView;

class AssistantContainerView : public views::BubbleDialogDelegateView,
                               public AssistantUiModelObserver,
                               public display::DisplayObserver,
                               public gfx::AnimationDelegate,
                               public keyboard::KeyboardControllerObserver {
 public:
  explicit AssistantContainerView(AssistantController* assistant_controller);
  ~AssistantContainerView() override;

  // Instructs the event targeter for the Assistant window to only allow mouse
  // click events to reach the specified |window|. All other events will not
  // be explored by |window|'s subtree for handling.
  static void OnlyAllowMouseClickEvents(aura::Window* window);

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  int GetDialogButtons() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void PreferredSizeChanged() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void Init() override;
  void RequestFocus() override;

  // AssistantUiModelObserver:
  void OnUiModeChanged(AssistantUiMode ui_mode) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardWorkspaceDisplacingBoundsChanged(
      const gfx::Rect& new_bounds) override;

 private:
  // Sets anchor rect to |root_window|. If it's null,
  // result of GetRootWindowForNewWindows() will be used.
  void SetAnchor(aura::Window* root_window);

  // Update the shadow layer.
  void UpdateShadow();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  AssistantMainView* assistant_main_view_;  // Owned by view hierarchy.
  AssistantMiniView* assistant_mini_view_;  // Owned by view hierarchy.
  AssistantWebView* assistant_web_view_;    // Owned by view hierarchy.

  std::unique_ptr<gfx::SlideAnimation> resize_animation_;
  gfx::SizeF resize_start_;
  gfx::SizeF resize_end_;

  // Cache the corner radius start value.
  int radius_start_ = 0;

  // Cache the corner radius target value.
  int radius_end_ = 0;

  // ui::LayerDelegate to paint rounded rectangle with shadow.
  std::unique_ptr<views::BorderShadowLayerDelegate> border_shadow_delegate_;

  // This layer shows a rounded rectangle with drop shadow.
  ui::Layer shadow_layer_;

  int animation_start_frame_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_H_
