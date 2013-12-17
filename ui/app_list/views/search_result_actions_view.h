// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_
#define UI_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_

#include "base/basictypes.h"
#include "ui/app_list/search_result.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace app_list {

class SearchResultActionsViewDelegate;

// SearchResultActionsView displays a SearchResult::Actions in a button
// strip. Each action is presented as a button and horizontally laid out.
class SearchResultActionsView : public views::View,
                                public views::ButtonListener {
 public:
  explicit SearchResultActionsView(SearchResultActionsViewDelegate* delegate);
  virtual ~SearchResultActionsView();

  void SetActions(const SearchResult::Actions& actions);

  void SetSelectedAction(int action_index);
  int selected_action() const { return selected_action_; }

  bool IsValidActionIndex(int action_index) const;

 private:
  void CreateImageButton(const SearchResult::Action& action);
  void CreateBlueButton(const SearchResult::Action& action);

  // views::View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  SearchResultActionsViewDelegate* delegate_;  // Not owned.
  int selected_action_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultActionsView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_
