// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class ImageView;
class Label;
class Widget;
}  // namespace views

class Tab;
struct TabRendererData;

// Dialog that displays an informational hover card containing page information.
class TabHoverCardBubbleView : public views::BubbleDialogDelegateView {
 public:
  explicit TabHoverCardBubbleView(Tab* tab);

  ~TabHoverCardBubbleView() override;

  // Updates card content and anchoring and shows the tab hover card.
  void UpdateAndShow(Tab* tab);

  void Hide();

  // BubbleDialogDelegateView:
  int GetDialogButtons() const override;

 private:
  friend class TabHoverCardBubbleViewBrowserTest;

  base::OneShotTimer delayed_show_timer_;

  views::Widget* widget_ = nullptr;
  views::Label* title_label_;
  views::Label* domain_label_;
  views::ImageView* preview_image_ = nullptr;

  // Get delay in milliseconds based on tab width.
  base::TimeDelta GetDelay(int tab_width) const;

  void ShowImmediately();

  // Updates and formats title and domain with given data.
  void UpdateCardContent(TabRendererData data);

  gfx::Size CalculatePreferredSize() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_BUBBLE_VIEW_H_
