// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/side_search/default_search_icon_source.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

// A search icon that appears when the side search feature is available. Opens
// the side panel to the available SRP.
class SideSearchIconView : public PageActionIconView {
 public:
  METADATA_HEADER(SideSearchIconView);
  explicit SideSearchIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate,
      Browser* browser);
  SideSearchIconView(const SideSearchIconView&) = delete;
  SideSearchIconView& operator=(const SideSearchIconView&) = delete;
  ~SideSearchIconView() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  ui::ImageModel GetSizedIconImage(int size) const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  raw_ptr<Browser> browser_ = nullptr;

  // Source for the default search icon image used by this page action.
  DefaultSearchIconSource default_search_icon_source_;

  // Animates out the side search icon label after a fixed period of time. This
  // keeps the label visible for long enough to give users an opportunity to
  // read the label text.
  base::OneShotTimer animate_out_timer_;

  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_
