// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/prefs/pref_member.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class CommandUpdater;

// The star icon to show a bookmark bubble.
class StarView : public PageActionIconView, public views::WidgetObserver {
 public:
  StarView(CommandUpdater* command_updater,
           Browser* browser,
           IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
           PageActionIconView::Delegate* page_action_icon_delegate);
  ~StarView() override;

  // Shows the BookmarkPromoBubbleView when the BookmarkTracker calls for it.
  void ShowPromo();

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  void ExecuteCommand(ExecuteSource source) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  SkColor GetInkDropBaseColor() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;
  const char* GetClassName() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void EditBookmarksPrefUpdated();
  bool IsBookmarkStarHiddenByExtension() const;

  Browser* const browser_;

  BooleanPrefMember edit_bookmarks_enabled_;

  // Observes the BookmarkPromoBubbleView's widget. Used to tell whether the
  // promo is open and gets called back when it closes.
  ScopedObserver<views::Widget, views::WidgetObserver> bookmark_promo_observer_{
      this};

  DISALLOW_COPY_AND_ASSIGN(StarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
