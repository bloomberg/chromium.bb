// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCOUNT_ICON_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCOUNT_ICON_CONTAINER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class AvatarToolbarButton;
class Browser;
class BrowserView;
class PageActionIconController;

// A container view for user-account-related PageActionIconViews and the profile
// avatar icon.
class ToolbarAccountIconContainerView : public ToolbarIconContainerView,
                                        public IconLabelBubbleView::Delegate,
                                        public PageActionIconContainer,
                                        public PageActionIconView::Delegate {
 public:
  METADATA_HEADER(ToolbarAccountIconContainerView);
  explicit ToolbarAccountIconContainerView(BrowserView* browser_view);
  ToolbarAccountIconContainerView(const ToolbarAccountIconContainerView&) =
      delete;
  ToolbarAccountIconContainerView& operator=(
      const ToolbarAccountIconContainerView&) = delete;
  ~ToolbarAccountIconContainerView() override;

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleInkDropColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // PageActionIconView::Delegate:
  float GetPageActionInkDropVisibleOpacity() const override;
  content::WebContents* GetWebContentsForPageActionIconView() override;
  gfx::Insets GetPageActionIconInsets(
      const PageActionIconView* icon_view) const override;

  // views::View:
  void OnThemeChanged() override;

  PageActionIconController* page_action_icon_controller() {
    return page_action_icon_controller_.get();
  }
  AvatarToolbarButton* avatar_button() { return avatar_; }

 private:
  // PageActionIconContainer:
  void AddPageActionIcon(std::unique_ptr<views::View> icon) override;

  std::unique_ptr<PageActionIconController> page_action_icon_controller_;

  const raw_ptr<AvatarToolbarButton> avatar_ = nullptr;

  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCOUNT_ICON_CONTAINER_VIEW_H_
