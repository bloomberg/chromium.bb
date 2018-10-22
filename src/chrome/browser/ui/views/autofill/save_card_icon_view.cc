// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardIconView::SaveCardIconView(CommandUpdater* command_updater,
                                   Browser* browser,
                                   PageActionIconView::Delegate* delegate,
                                   const gfx::FontList& font_list)
    : PageActionIconView(command_updater,
                         IDC_SAVE_CREDIT_CARD_FOR_PAGE,
                         delegate,
                         font_list),
      browser_(browser) {
  DCHECK(delegate);
  set_id(VIEW_ID_SAVE_CREDIT_CARD_BUTTON);

  SetUpForInOutAnimation();
}

SaveCardIconView::~SaveCardIconView() {}

views::BubbleDialogDelegateView* SaveCardIconView::GetBubble() const {
  SaveCardBubbleControllerImpl* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::SaveCardBubbleViews*>(
      controller->save_card_bubble_view());
}

bool SaveCardIconView::Update() {
  if (!GetWebContents())
    return false;

  const bool was_visible = visible();

  // |controller| may be nullptr due to lazy initialization.
  SaveCardBubbleControllerImpl* controller = GetController();
  bool enabled = controller && controller->IsIconVisible();

  enabled &= SetCommandEnabled(enabled);
  SetVisible(enabled);

  if (enabled && controller->CanAnimate()) {
    AnimateIn(IDS_AUTOFILL_CARD_SAVED);
  }

  return was_visible != visible();
}

void SaveCardIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SaveCardIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

base::string16 SaveCardIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD);
}

SaveCardBubbleControllerImpl* SaveCardIconView::GetController() const {
  if (!browser_)
    return nullptr;
  content::WebContents* web_contents = GetWebContents();

  if (!web_contents)
    return nullptr;
  return autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
}

bool SaveCardIconView::ShouldShowSeparator() const {
  return false;
}

void SaveCardIconView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  // |controller| may be nullptr due to lazy initialization.
  SaveCardBubbleControllerImpl* controller = GetController();
  if (controller)
    controller->OnAnimationEnded();
}

}  // namespace autofill
