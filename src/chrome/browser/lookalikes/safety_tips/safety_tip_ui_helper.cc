// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/safety_tip_ui_helper.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

// URL that the "leave site" button aborts to.
const char kSafeUrl[] = "chrome://newtab";

}  // namespace

namespace safety_tips {

void RecordSafetyTipInteractionHistogram(content::WebContents* web_contents,
                                         SafetyTipInteraction interaction) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  base::UmaHistogramEnumeration(
      security_state::GetSafetyTipHistogramName(
          "Security.SafetyTips.Interaction",
          helper->GetVisibleSecurityState()->safety_tip_status),
      interaction);
}

void LeaveSite(content::WebContents* web_contents) {
  RecordSafetyTipInteractionHistogram(web_contents,
                                      SafetyTipInteraction::kLeaveSite);
  content::OpenURLParams params(
      GURL(kSafeUrl), content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */);
  params.should_replace_current_entry = true;
  web_contents->OpenURL(params);
}

int GetSafetyTipTitleId(security_state::SafetyTipStatus warning_type) {
  switch (warning_type) {
    case security_state::SafetyTipStatus::kBadReputation:
    case security_state::SafetyTipStatus::kLookalike:
#if defined(OS_ANDROID)
      return IDS_SAFETY_TIP_ANDROID_BAD_REPUTATION_TITLE;
#else
      return IDS_PAGE_INFO_SAFETY_TIP_SUMMARY;
#endif
    case security_state::SafetyTipStatus::kUnknown:
    case security_state::SafetyTipStatus::kNone:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

int GetSafetyTipDescriptionId(security_state::SafetyTipStatus warning_type) {
  switch (warning_type) {
    case security_state::SafetyTipStatus::kBadReputation:
#if defined(OS_ANDROID)
      return IDS_SAFETY_TIP_ANDROID_BAD_REPUTATION_DESCRIPTION;
#else
      return IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_DESCRIPTION;
#endif
    case security_state::SafetyTipStatus::kLookalike:
      return IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH;
    case security_state::SafetyTipStatus::kNone:
    case security_state::SafetyTipStatus::kUnknown:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

}  // namespace safety_tips
