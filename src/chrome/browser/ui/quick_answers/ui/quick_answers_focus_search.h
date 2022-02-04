// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_FOCUS_SEARCH_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_FOCUS_SEARCH_H_

#include "base/callback.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"

// This class manages the focus traversal order for elements inside
// Quick-Answers related views.
// TODO(siabhijeet): QuickAnswersView is a menu-companion, so ideally should
// avoid disturbing existing focus. Explore other ways for keyboard
// accessibility.
class QuickAnswersFocusSearch : public views::FocusSearch,
                                public views::FocusTraversable {
 public:
  using GetFocusableViewsCallback =
      base::RepeatingCallback<std::vector<views::View*>(void)>;

  explicit QuickAnswersFocusSearch(views::View* view,
                                   const GetFocusableViewsCallback& callback);

  ~QuickAnswersFocusSearch() override;

  // views::FocusSearch:
  views::View* FindNextFocusableView(
      views::View* starting_view,
      SearchDirection search_direction,
      TraversalDirection traversal_direction,
      StartingViewPolicy check_starting_view,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

 private:
  views::View* const view_;
  const GetFocusableViewsCallback get_focusable_views_callback_;
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_FOCUS_SEARCH_H_
