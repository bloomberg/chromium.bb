// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

using browser_command::mojom::ClickInfoPtr;
using browser_command::mojom::Command;
using browser_command::mojom::CommandHandler;

// static
const char BrowserCommandHandler::kPromoBrowserCommandHistogramName[] =
    "NewTabPage.Promos.PromoBrowserCommand";

BrowserCommandHandler::BrowserCommandHandler(
    mojo::PendingReceiver<CommandHandler> pending_page_handler,
    Profile* profile,
    std::vector<browser_command::mojom::Command> supported_commands)
    : profile_(profile),
      supported_commands_(supported_commands),
      command_updater_(std::make_unique<CommandUpdaterImpl>(this)),
      page_handler_(this, std::move(pending_page_handler)) {
  if (supported_commands_.empty())
    return;

  EnableSupportedCommands();
}

BrowserCommandHandler::~BrowserCommandHandler() = default;

void BrowserCommandHandler::CanExecuteCommand(
    browser_command::mojom::Command command_id,
    CanExecuteCommandCallback callback) {
  if (!base::Contains(supported_commands_, command_id)) {
    std::move(callback).Run(false);
    return;
  }

  bool can_execute = false;
  switch (static_cast<Command>(command_id)) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    case Command::kOpenSafetyCheck:
      can_execute = !chrome::enterprise_util::IsBrowserManaged(profile_);
      break;
    case Command::kOpenSafeBrowsingEnhancedProtectionSettings: {
      bool managed = safe_browsing::SafeBrowsingPolicyHandler::
          IsSafeBrowsingProtectionLevelSetByPolicy(profile_->GetPrefs());
      bool already_enabled =
          safe_browsing::IsEnhancedProtectionEnabled(*(profile_->GetPrefs()));
      can_execute = !managed && !already_enabled;
    } break;
    case Command::kOpenFeedbackForm:
      can_execute = true;
      break;
    case Command::kOpenPrivacyReview:
      can_execute = base::FeatureList::IsEnabled(features::kPrivacyReview) &&
                    !chrome::enterprise_util::IsBrowserManaged(profile_) &&
                    !profile_->IsChild();
      break;
    default:
      NOTREACHED() << "Unspecified behavior for command " << command_id;
      break;
  }
  std::move(callback).Run(can_execute);
}

void BrowserCommandHandler::ExecuteCommand(Command command_id,
                                           ClickInfoPtr click_info,
                                           ExecuteCommandCallback callback) {
  if (!base::Contains(supported_commands_, command_id)) {
    std::move(callback).Run(false);
    return;
  }

  const auto disposition = ui::DispositionFromClick(
      click_info->middle_button, click_info->alt_key, click_info->ctrl_key,
      click_info->meta_key, click_info->shift_key);
  const bool command_executed =
      GetCommandUpdater()->ExecuteCommandWithDisposition(
          static_cast<int>(command_id), disposition);
  std::move(callback).Run(command_executed);
}

void BrowserCommandHandler::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  const auto command = static_cast<Command>(id);
  base::UmaHistogramEnumeration(kPromoBrowserCommandHistogramName, command);

  switch (command) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    case Command::kOpenSafetyCheck:
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kSafetyCheckSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_SafetyCheck"));
      break;
    case Command::kOpenSafeBrowsingEnhancedProtectionSettings:
      NavigateToURL(GURL(chrome::GetSettingsUrl(
                        chrome::kSafeBrowsingEnhancedProtectionSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_EnhancedProtection"));
      break;
    case Command::kOpenFeedbackForm:
      OpenFeedbackForm();
      break;
    case Command::kOpenPrivacyReview:
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kPrivacyReviewSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_PrivacyGuide"));
      break;
    default:
      NOTREACHED() << "Unspecified behavior for command " << id;
      break;
  }
}

void BrowserCommandHandler::OpenFeedbackForm() {
  chrome::ShowFeedbackPage(feedback_settings_.url, profile_,
                           feedback_settings_.source,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           feedback_settings_.category /* category_tag */,
                           std::string() /* extra_diagnostics */);
}

void BrowserCommandHandler::ConfigureFeedbackCommand(
    FeedbackCommandSettings settings) {
  feedback_settings_ = settings;
}

void BrowserCommandHandler::EnableSupportedCommands() {
  // Explicitly enable supported commands.
  GetCommandUpdater()->UpdateCommandEnabled(
      static_cast<int>(Command::kUnknownCommand), true);
  for (Command command : supported_commands_) {
    GetCommandUpdater()->UpdateCommandEnabled(static_cast<int>(command), true);
  }
}

CommandUpdater* BrowserCommandHandler::GetCommandUpdater() {
  return command_updater_.get();
}

void BrowserCommandHandler::NavigateToURL(const GURL& url,
                                          WindowOpenDisposition disposition) {
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  Navigate(&params);
}
