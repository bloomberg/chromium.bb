// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ash {
class AssistantViewDelegate;
class AssistantWebView;
}  // namespace ash

namespace app_list {

class AssistantMainView;

// The Assistant page for the app list.
class APP_LIST_EXPORT AssistantPageView : public AppListPage,
                                          public ash::AssistantUiModelObserver {
 public:
  explicit AssistantPageView(
      ash::AssistantViewDelegate* assistant_view_delegate);
  ~AssistantPageView() override;

  void InitLayout();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListPage:
  gfx::Rect GetPageBoundsForState(ash::AppListState state) const override;
  gfx::Rect GetSearchBoxBounds() const override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;

  // AssistantUiModelObserver:
  void OnUiModeChanged(ash::AssistantUiMode ui_mode) override;
  void OnUiVisibilityChanged(
      ash::AssistantVisibility new_visibility,
      ash::AssistantVisibility old_visibility,
      base::Optional<ash::AssistantEntryPoint> entry_point,
      base::Optional<ash::AssistantExitPoint> exit_point) override;

  gfx::Rect AddShadowBorderToBounds(const gfx::Rect& bounds) const;

 private:
  ash::AssistantViewDelegate* const assistant_view_delegate_;

  // Owned by the views hierarchy.
  AssistantMainView* assistant_main_view_ = nullptr;
  ash::AssistantWebView* assistant_web_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantPageView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_PAGE_VIEW_H_
