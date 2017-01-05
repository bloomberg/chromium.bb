// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/app_list/views/search_result_container_view.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class LinearAnimation;
}

namespace app_list {
namespace test {
class SearchResultListViewTest;
}

class AppListViewDelegate;
class SearchResultListViewDelegate;
class SearchResultView;

// SearchResultListView displays SearchResultList with a list of
// SearchResultView.
class APP_LIST_EXPORT SearchResultListView : public gfx::AnimationDelegate,
                                             public SearchResultContainerView {
 public:
  SearchResultListView(SearchResultListViewDelegate* delegate,
                       AppListViewDelegate* view_delegate);
  ~SearchResultListView() override;

  void UpdateAutoLaunchState();

  bool IsResultViewSelected(const SearchResultView* result_view) const;

  void SearchResultActivated(SearchResultView* view, int event_flags);

  void SearchResultActionActivated(SearchResultView* view,
                                   size_t action_index,
                                   int event_flags);

  void OnSearchResultInstalled(SearchResultView* view);

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  gfx::Size GetPreferredSize() const override;

  // Overridden from ui::ListModelObserver:
  void ListItemsRemoved(size_t start, size_t count) override;

  // Overridden from SearchResultContainerView:
  void OnContainerSelected(bool from_bottom,
                           bool directional_movement) override;
  void NotifyFirstResultYIndex(int y_index) override;
  int GetYSize() override;

 private:
  friend class test::SearchResultListViewTest;

  // Overridden from SearchResultContainerView:
  int DoUpdate() override;
  void UpdateSelectedIndex(int old_selected, int new_selected) override;

  // Updates the auto launch states.
  void SetAutoLaunchTimeout(const base::TimeDelta& timeout);
  void CancelAutoLaunchTimeout();

  // Helper function to get SearchResultView at given |index|.
  SearchResultView* GetResultViewAt(int index);

  // Forcibly auto-launch for test if it is in auto-launching state.
  void ForceAutoLaunchForTest();

  // Overridden from views::View:
  void Layout() override;
  int GetHeightForWidth(int w) const override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  SearchResultListViewDelegate* delegate_;  // Not owned.
  AppListViewDelegate* view_delegate_;  // Not owned.

  views::View* results_container_;
  views::View* auto_launch_indicator_;
  std::unique_ptr<gfx::LinearAnimation> auto_launch_animation_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultListView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
