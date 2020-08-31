// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

class CommandUpdater;

namespace autofill {

class SavePaymentIconController;

// The location bar icon to show the Save Credit Card bubble where the user can
// choose to save the credit card info to use again later without re-entering
// it.
class SavePaymentIconView : public PageActionIconView {
 public:
  SavePaymentIconView(CommandUpdater* command_updater,
                      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                      PageActionIconView::Delegate* page_action_icon_delegate);
  ~SavePaymentIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegateView* GetBubble() const override;
  void UpdateImpl() override;
  base::string16 GetTextForTooltipAndAccessibleName() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  const gfx::VectorIcon& GetVectorIconBadge() const override;
  const char* GetClassName() const override;

 private:
  SavePaymentIconController* GetController() const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

  DISALLOW_COPY_AND_ASSIGN(SavePaymentIconView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_VIEW_H_
