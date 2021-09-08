// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/search_result_actions_view_delegate.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/context_menu_controller.h"

namespace gfx {
class RenderText;
}

namespace views {
class ImageView;
}  // namespace views

namespace ash {
namespace test {
class SearchResultListViewTest;
}  // namespace test

class AppListViewDelegate;
class SearchResult;
class SearchResultListView;

// SearchResultView displays a SearchResult.
class ASH_EXPORT SearchResultView : public SearchResultBaseView,
                                    public views::ContextMenuController,
                                    public SearchResultActionsViewDelegate {
 public:
  // Internal class name.
  static const char kViewClassName[];

  explicit SearchResultView(SearchResultListView* list_view,
                            AppListViewDelegate* view_delegate);
  ~SearchResultView() override;

  // Sets/gets SearchResult displayed by this view.
  void OnResultChanged() override;

 private:
  friend class test::SearchResultListViewTest;

  void UpdateTitleText();
  void UpdateDetailsText();

  // Creates title/details render text.
  void CreateTitleRenderText();
  void CreateDetailsRenderText();

  // Callback for query suggstion removal confirmation.
  void OnQueryRemovalAccepted(bool accepted);

  // views::View overrides:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Bound by ShowContextMenuForViewImpl().
  void OnGetContextMenu(views::View* source,
                        const gfx::Point& point,
                        ui::MenuSourceType source_type,
                        std::unique_ptr<ui::SimpleMenuModel> menu_model);

  // SearchResultObserver overrides:
  void OnMetadataChanged() override;

  void OnButtonPressed(const ui::Event& event);

  void SetIconImage(const gfx::ImageSkia& source,
                    views::ImageView* const icon,
                    const gfx::Size& size);

  // SearchResultActionsViewDelegate overrides:
  void OnSearchResultActionActivated(size_t index) override;
  bool IsSearchResultHoveredOrSelected() override;

  // Invoked when the context menu closes.
  void OnMenuClosed();

  // Whether this result has a rich image icon.
  bool IsRichImage() const;

  // Parent list view. Owned by views hierarchy.
  SearchResultListView* list_view_;

  AppListViewDelegate* view_delegate_;

  views::ImageView* icon_;  // Owned by views hierarchy.
  // Rich image results will show |image_icon_| instead of |icon_|.
  views::ImageView* image_icon_;  // Owned by views hierarchy.
  views::ImageView* badge_icon_;  // Owned by views hierarchy.
  std::unique_ptr<gfx::RenderText> title_text_;
  std::unique_ptr<gfx::RenderText> details_text_;

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  // Whether the removal confirmation dialog is invoked by long press touch.
  bool confirm_remove_by_long_press_ = false;

  base::WeakPtrFactory<SearchResultView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchResultView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_VIEW_H_
