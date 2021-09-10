// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

// The location bar icon to show the Translate bubble where the user can have
// the page translated.
class TranslateIconView : public PageActionIconView {
 public:
  METADATA_HEADER(TranslateIconView);
  TranslateIconView(CommandUpdater* command_updater,
                    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                    PageActionIconView::Delegate* page_action_icon_delegate);
  TranslateIconView(const TranslateIconView&) = delete;
  TranslateIconView& operator=(const TranslateIconView&) = delete;
  ~TranslateIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  void OnPressed(bool activated) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_
