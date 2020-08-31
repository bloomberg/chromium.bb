// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

LocalCardMigrationIconView::LocalCardMigrationIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_MIGRATE_LOCAL_CREDIT_CARD_FOR_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {
  SetID(VIEW_ID_MIGRATE_LOCAL_CREDIT_CARD_BUTTON);
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback)) {
    InstallLoadingIndicator();
  } else {
    SetUpForInOutAnimation();
  }
}

LocalCardMigrationIconView::~LocalCardMigrationIconView() {}

views::BubbleDialogDelegateView* LocalCardMigrationIconView::GetBubble() const {
  ManageMigrationUiController* controller = GetController();
  if (!controller)
    return nullptr;

  LocalCardMigrationFlowStep step = controller->GetFlowStep();
  DCHECK_NE(step, LocalCardMigrationFlowStep::UNKNOWN);
  switch (step) {
    case LocalCardMigrationFlowStep::PROMO_BUBBLE:
      return static_cast<LocalCardMigrationBubbleViews*>(
          controller->GetBubbleView());
    case LocalCardMigrationFlowStep::NOT_SHOWN:
    case LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING:
      return nullptr;
    default:
      return static_cast<LocalCardMigrationDialogView*>(
          controller->GetDialogView());
  }
}

void LocalCardMigrationIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  // |controller| may be nullptr due to lazy initialization.
  ManageMigrationUiController* controller = GetController();
  bool enabled = controller && controller->IsIconVisible();
  enabled &= SetCommandEnabled(enabled);
  SetVisible(enabled);

  if (GetVisible()) {
    switch (controller->GetFlowStep()) {
      // When the dialog is about to show, trigger the ink drop animation
      // so that the credit card icon in "selected" state by default. This needs
      // to be manually set since the migration dialog is not anchored at the
      // credit card icon.
      case LocalCardMigrationFlowStep::OFFER_DIALOG: {
        UpdateIconImage();
        SetHighlighted(true);
        break;
      }
      case LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING: {
        SetHighlighted(false);
        // Disable the credit card icon so it does not update if user clicks
        // on it.
        SetEnabled(false);
        if (base::FeatureList::IsEnabled(
                features::kAutofillCreditCardUploadFeedback)) {
          SetIsLoading(/*is_loading=*/true);
        } else {
          AnimateIn(IDS_AUTOFILL_LOCAL_CARD_MIGRATION_ANIMATION_LABEL);
        }
        break;
      }
      case LocalCardMigrationFlowStep::MIGRATION_FINISHED: {
        if (base::FeatureList::IsEnabled(
                features::kAutofillCreditCardUploadFeedback)) {
          SetIsLoading(/*is_loading=*/false);
        } else {
          UnpauseAnimation();
        }
        SetEnabled(true);
        break;
      }
      case LocalCardMigrationFlowStep::MIGRATION_FAILED: {
        if (base::FeatureList::IsEnabled(
                features::kAutofillCreditCardUploadFeedback)) {
          UpdateIconImage();
          SetIsLoading(/*is_loading=*/false);
        } else {
          UnpauseAnimation();
        }
        SetEnabled(true);
        break;
      }
      default:
        break;
    }
  } else {
    SetHighlighted(false);
    // Handle corner cases where users navigate away or close the tab.
    UnpauseAnimation();
  }
}

void LocalCardMigrationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& LocalCardMigrationIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

const gfx::VectorIcon& LocalCardMigrationIconView::GetVectorIconBadge() const {
  ManageMigrationUiController* controller = GetController();
  if (controller && controller->GetFlowStep() ==
                        LocalCardMigrationFlowStep::MIGRATION_FAILED) {
    return vector_icons::kBlockedBadgeIcon;
  }
  return gfx::kNoneIcon;
}

const char* LocalCardMigrationIconView::GetClassName() const {
  return "LocalCardMigrationIconView";
}

base::string16 LocalCardMigrationIconView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_MIGRATE_LOCAL_CARD);
}

ManageMigrationUiController* LocalCardMigrationIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  return autofill::ManageMigrationUiController::FromWebContents(web_contents);
}

void LocalCardMigrationIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationProgressed(animation);

  // Pause the animation when the animation text is completely shown. If the
  // user navigates to other tab when the animation is in progress, don't pause
  // the animation.
  constexpr double animation_text_full_length_shown_state = 0.5;
  if (GetController() &&
      GetController()->GetFlowStep() ==
          LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING &&
      GetAnimationValue() >= animation_text_full_length_shown_state) {
    PauseAnimation();
  }
}

void LocalCardMigrationIconView::AnimationEnded(
    const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);
  UpdateIconImage();
}

}  // namespace autofill
