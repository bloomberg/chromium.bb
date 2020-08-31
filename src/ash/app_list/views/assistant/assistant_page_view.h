// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"

namespace ash {

class AssistantMainView;
class AssistantViewDelegate;
class ContentsView;
class ViewShadow;

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage,
                                          public AssistantControllerObserver,
                                          public AssistantUiModelObserver {
 public:
  AssistantPageView(AssistantViewDelegate* assistant_view_delegate,
                    ContentsView* contents_view);
  ~AssistantPageView() override;

  void InitLayout();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListPage:
  void OnShown() override;
  void OnAnimationStarted(AppListState from_state,
                          AppListState to_state) override;
  base::Optional<int> GetSearchBoxTop(
      AppListViewState view_state) const override;
  void UpdatePageOpacityForState(AppListState state,
                                 float search_box_opacity,
                                 bool restore_opacity) override;
  gfx::Rect GetPageBoundsForState(
      AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;
  void AnimateYPosition(AppListViewState target_view_state,
                        const TransformAnimator& animator,
                        float default_offset) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  int GetChildViewHeightForWidth(int width) const;
  void MaybeUpdateAppListState(int child_height);
  gfx::Rect AddShadowBorderToBounds(const gfx::Rect& bounds) const;

  AssistantViewDelegate* const assistant_view_delegate_;
  ContentsView* const contents_view_;

  // Owned by the view hierarchy.
  AssistantMainView* assistant_main_view_ = nullptr;

  int min_height_dip_;

  std::unique_ptr<ViewShadow> view_shadow_;

  ScopedObserver<AssistantController, AssistantControllerObserver>
      assistant_controller_observer_{this};

  ScopedObserver<AssistantUiController,
                 AssistantUiModelObserver,
                 &AssistantUiController::AddModelObserver,
                 &AssistantUiController::RemoveModelObserver>
      assistant_ui_model_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
