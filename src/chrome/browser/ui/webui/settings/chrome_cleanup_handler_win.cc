// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler_win.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/grit/generated_resources.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"

using safe_browsing::ChromeCleanerController;

namespace settings {

namespace {

// Returns a ListValue containing a copy of the file paths stored in |files|.
std::unique_ptr<base::ListValue> GetFilesAsListStorage(
    const std::set<base::FilePath>& files) {
  auto value = std::make_unique<base::ListValue>();
  for (const base::FilePath& path : files) {
    auto item = std::make_unique<base::DictionaryValue>();
    item->SetString("dirname",
                    path.DirName().AsEndingWithSeparator().AsUTF8Unsafe());
    item->SetString("basename", path.BaseName().AsUTF8Unsafe());
    value->Append(std::move(item));
  }
  return value;
}

// Returns a ListValue containing a copy of the strings stored in |string_set|.
std::unique_ptr<base::ListValue> GetStringSetAsListStorage(
    const std::set<std::wstring>& string_set) {
  auto value = std::make_unique<base::ListValue>();
  for (const std::wstring& string : string_set)
    value->Append(base::AsString16(string));

  return value;
}

base::DictionaryValue GetScannerResultsAsDictionary(
    const safe_browsing::ChromeCleanerScannerResults& scanner_results,
    Profile* profile) {
  base::DictionaryValue value;
  value.SetList("files",
                GetFilesAsListStorage(scanner_results.files_to_delete()));
  value.SetList("registryKeys",
                GetStringSetAsListStorage(scanner_results.registry_keys()));
  return value;
}

std::string IdleReasonToString(
    ChromeCleanerController::IdleReason idle_reason) {
  switch (idle_reason) {
    case ChromeCleanerController::IdleReason::kInitial:
      return "initial";
    case ChromeCleanerController::IdleReason::kReporterFoundNothing:
      return "reporter_found_nothing";
    case ChromeCleanerController::IdleReason::kReporterFailed:
      return "reporter_failed";
    case ChromeCleanerController::IdleReason::kScanningFoundNothing:
      return "scanning_found_nothing";
    case ChromeCleanerController::IdleReason::kScanningFailed:
      return "scanning_failed";
    case ChromeCleanerController::IdleReason::kConnectionLost:
      return "connection_lost";
    case ChromeCleanerController::IdleReason::kUserDeclinedCleanup:
      return "user_declined_cleanup";
    case ChromeCleanerController::IdleReason::kCleaningFailed:
      return "cleaning_failed";
    case ChromeCleanerController::IdleReason::kCleaningSucceeded:
      return "cleaning_succeeded";
    case ChromeCleanerController::IdleReason::kCleanerDownloadFailed:
      return "cleaner_download_failed";
  }
  NOTREACHED();
  return "";
}

}  // namespace

ChromeCleanupHandler::ChromeCleanupHandler(Profile* profile)
    : controller_(ChromeCleanerController::GetInstance()), profile_(profile) {}

ChromeCleanupHandler::~ChromeCleanupHandler() {
  controller_->RemoveObserver(this);
}

void ChromeCleanupHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "registerChromeCleanerObserver",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleRegisterChromeCleanerObserver,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "startScanning",
      base::BindRepeating(&ChromeCleanupHandler::HandleStartScanning,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "restartComputer",
      base::BindRepeating(&ChromeCleanupHandler::HandleRestartComputer,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "startCleanup",
      base::BindRepeating(&ChromeCleanupHandler::HandleStartCleanup,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "notifyShowDetails",
      base::BindRepeating(&ChromeCleanupHandler::HandleNotifyShowDetails,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "notifyChromeCleanupLearnMoreClicked",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleNotifyChromeCleanupLearnMoreClicked,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getMoreItemsPluralString",
      base::BindRepeating(&ChromeCleanupHandler::HandleGetMoreItemsPluralString,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "getItemsToRemovePluralString",
      base::BindRepeating(
          &ChromeCleanupHandler::HandleGetItemsToRemovePluralString,
          base::Unretained(this)));
}

void ChromeCleanupHandler::OnJavascriptAllowed() {
  controller_->AddObserver(this);
}

void ChromeCleanupHandler::OnJavascriptDisallowed() {
  controller_->RemoveObserver(this);
}

void ChromeCleanupHandler::OnIdle(
    ChromeCleanerController::IdleReason idle_reason) {
  FireWebUIListener("chrome-cleanup-on-idle",
                    base::Value(IdleReasonToString(idle_reason)));
}

void ChromeCleanupHandler::OnScanning() {
  FireWebUIListener("chrome-cleanup-on-scanning");
}

void ChromeCleanupHandler::OnReporterRunning() {
  FireWebUIListener("chrome-cleanup-on-reporter-running");
}

void ChromeCleanupHandler::OnInfected(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  FireWebUIListener("chrome-cleanup-on-infected",
                    base::Value(is_powered_by_partner),
                    GetScannerResultsAsDictionary(scanner_results, profile_));
}

void ChromeCleanupHandler::OnCleaning(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  FireWebUIListener("chrome-cleanup-on-cleaning",
                    base::Value(is_powered_by_partner),
                    GetScannerResultsAsDictionary(scanner_results, profile_));
}

void ChromeCleanupHandler::OnRebootRequired() {
  FireWebUIListener("chrome-cleanup-on-reboot-required");
}

void ChromeCleanupHandler::HandleRegisterChromeCleanerObserver(
    const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetList().size());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_Shown"));
  AllowJavascript();

  FireWebUIListener("chrome-cleanup-enabled-change",
                    base::Value(controller_->IsAllowedByPolicy()));
}

void ChromeCleanupHandler::HandleStartScanning(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  bool allow_logs_upload = false;
  if (args->GetList()[0].is_bool())
    allow_logs_upload = args->GetList()[0].GetBool();

  // If this operation is not allowed the UI should be disabled.
  CHECK(controller_->IsAllowedByPolicy());

  // The state is propagated to all open tabs and should be consistent.
  DCHECK_EQ(controller_->logs_enabled(profile_), allow_logs_upload);

  controller_->RequestUserInitiatedScan(profile_);

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_StartScanning"));
}

void ChromeCleanupHandler::HandleRestartComputer(const base::ListValue* args) {
  DCHECK_EQ(0U, args->GetList().size());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_RestartComputer"));

  controller_->Reboot();
}

void ChromeCleanupHandler::HandleStartCleanup(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  bool allow_logs_upload = false;
  if (args->GetList()[0].is_bool())
    allow_logs_upload = args->GetList()[0].GetBool();

  // The state is propagated to all open tabs and should be consistent.
  DCHECK_EQ(controller_->logs_enabled(profile_), allow_logs_upload);

  safe_browsing::RecordCleanupStartedHistogram(
      safe_browsing::CLEANUP_STARTED_FROM_PROMPT_IN_SETTINGS);
  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_StartCleanup"));

  controller_->ReplyWithUserResponse(
      profile_,
      allow_logs_upload
          ? ChromeCleanerController::UserResponse::kAcceptedWithLogs
          : ChromeCleanerController::UserResponse::kAcceptedWithoutLogs);
}

void ChromeCleanupHandler::HandleNotifyShowDetails(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  bool details_section_visible = false;
  if (args->GetList()[0].is_bool())
    details_section_visible = args->GetList()[0].GetBool();

  if (details_section_visible) {
    base::RecordAction(
        base::UserMetricsAction("SoftwareReporter.CleanupWebui_ShowDetails"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("SoftwareReporter.CleanupWebui_HideDetails"));
  }
}

void ChromeCleanupHandler::HandleNotifyChromeCleanupLearnMoreClicked(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());

  base::RecordAction(
      base::UserMetricsAction("SoftwareReporter.CleanupWebui_LearnMore"));
}

void ChromeCleanupHandler::HandleGetMoreItemsPluralString(
    const base::ListValue* args) {
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GetPluralString(IDS_SETTINGS_RESET_CLEANUP_DETAILS_MORE, args);
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ChromeCleanupHandler::HandleGetItemsToRemovePluralString(
    const base::ListValue* args) {
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  GetPluralString(IDS_SETTINGS_RESET_CLEANUP_DETAILS_ITEMS_TO_BE_REMOVED, args);
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ChromeCleanupHandler::GetPluralString(int id,
                                           const base::ListValue* args) {
  const auto& list = args->GetList();
  CHECK_EQ(2U, list.size());

  std::string callback_id = list[0].GetString();

  int num_items = list[1].GetIfInt().value_or(0);

  const std::u16string plural_string =
      num_items > 0 ? l10n_util::GetPluralStringFUTF16(id, num_items)
                    : std::u16string();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(plural_string));
}

}  // namespace settings
