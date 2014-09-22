// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_CONTENTS_SWITCHER_VIEW_H_
#define UI_APP_LIST_VIEWS_CONTENTS_SWITCHER_VIEW_H_

#include <map>

#include "base/basictypes.h"
#include "ui/app_list/pagination_model_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace app_list {

class ContentsView;

// A view that contains buttons to switch the displayed view in the given
// ContentsView.
class ContentsSwitcherView : public views::View,
                             public views::ButtonListener,
                             public PaginationModelObserver {
 public:
  explicit ContentsSwitcherView(ContentsView* contents_view);
  virtual ~ContentsSwitcherView();

  ContentsView* contents_view() const { return contents_view_; }

  // Adds a switcher button using |resource_id| as the button's image, which
  // opens the page with index |page_index| in the ContentsView.
  void AddSwitcherButton(int resource_id, int page_index);

 private:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from PaginationModelObserver:
  virtual void TotalPagesChanged() OVERRIDE;
  virtual void SelectedPageChanged(int old_selected, int new_selected) OVERRIDE;
  virtual void TransitionStarted() OVERRIDE;
  virtual void TransitionChanged() OVERRIDE;

  ContentsView* contents_view_;  // Owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(ContentsSwitcherView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_CONTENTS_SWITCHER_VIEW_H_
