// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

IntentChipButton::IntentChipButton(Browser* browser,
                                   PageActionIconView::Delegate* delegate)
    : OmniboxChipButton(base::BindRepeating(&IntentChipButton::HandlePressed,
                                            base::Unretained(this)),
                        vector_icons::kOpenInNewIcon,
                        vector_icons::kOpenInNewIcon,
                        l10n_util::GetStringUTF16(IDS_INTENT_CHIP_LABEL),
                        true),
      browser_(browser),
      delegate_(delegate) {
  SetTheme(Theme::kLowVisibility);
  ResetAnimation(1);
}

IntentChipButton::~IntentChipButton() = default;

void IntentChipButton::Update() {
  bool was_visible = GetVisible();
  bool is_visible = GetShowChip();
  SetVisible(is_visible);

  if (was_visible && !is_visible)
    IntentPickerBubbleView::CloseCurrentBubble();
}

bool IntentChipButton::GetShowChip() const {
  if (delegate_->ShouldHidePageActionIcons())
    return false;

  auto* tab_helper = GetTabHelper();
  return tab_helper && tab_helper->should_show_icon();
}

void IntentChipButton::HandlePressed() {
  content::WebContents* web_contents =
      delegate_->GetWebContentsForPageActionIconView();
  const GURL& url = web_contents->GetURL();
  apps::ShowIntentPickerBubble(web_contents, url);
}

IntentPickerTabHelper* IntentChipButton::GetTabHelper() const {
  if (browser_->profile()->IsOffTheRecord())
    return nullptr;

  content::WebContents* web_contents =
      delegate_->GetWebContentsForPageActionIconView();
  if (!web_contents)
    return nullptr;

  return IntentPickerTabHelper::FromWebContents(web_contents);
}

BEGIN_METADATA(IntentChipButton, OmniboxChipButton)
END_METADATA
