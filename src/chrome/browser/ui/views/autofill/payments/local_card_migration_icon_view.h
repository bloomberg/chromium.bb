// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace autofill {

class ManageMigrationUiController;

// The icon shown in location bar for the intermediate local card migration
// bubble.
class LocalCardMigrationIconView : public PageActionIconView {
 public:
  LocalCardMigrationIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);

  LocalCardMigrationIconView(const LocalCardMigrationIconView&) = delete;
  LocalCardMigrationIconView& operator=(const LocalCardMigrationIconView&) =
      delete;

  ~LocalCardMigrationIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  std::u16string GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const gfx::VectorIcon& GetVectorIconBadge() const override;
  const char* GetClassName() const override;

 private:
  ManageMigrationUiController* GetController() const;

  // IconLabelBubbleView:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_ICON_VIEW_H_
