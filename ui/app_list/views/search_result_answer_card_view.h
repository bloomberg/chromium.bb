// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_ANSWER_CARD_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_ANSWER_CARD_VIEW_H_

#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/views/search_result_container_view.h"

namespace app_list {

class AppListModel;
class SearchResultPageView;

// Result container for the search answer card.
class APP_LIST_EXPORT SearchResultAnswerCardView
    : public SearchResultContainerView,
      public AppListModelObserver {
 public:
  SearchResultAnswerCardView(AppListModel* model,
                             SearchResultPageView* search_results_page_view,
                             views::View* search_answer_view);
  ~SearchResultAnswerCardView() override;

 private:
  class SearchAnswerContainerView;

  // Overridden from views::View:
  const char* GetClassName() const override;

  // Overridden from SearchResultContainerView:
  void OnContainerSelected(bool from_bottom,
                           bool directional_movement) override;
  void NotifyFirstResultYIndex(int y_index) override {}
  int GetYSize() override;
  int DoUpdate() override;
  void UpdateSelectedIndex(int old_selected, int new_selected) override;

  // Overridden from AppListModelObserver
  void OnSearchAnswerAvailableChanged(bool has_answer) override;

  // Unowned pointer to application list model.
  AppListModel* const model_;

  // Pointer to the container of the search answer; owned by the view hierarchy.
  // It's visible iff we have a search answer result.
  SearchAnswerContainerView* const search_answer_container_view_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultAnswerCardView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_ANSWER_CARD_VIEW_H_
