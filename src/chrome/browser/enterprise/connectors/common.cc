// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace enterprise_connectors {

namespace {
constexpr char kDlpTag[] = "dlp";
constexpr char kMalwareTag[] = "malware";
}  // namespace

AnalysisSettings::AnalysisSettings() = default;
AnalysisSettings::AnalysisSettings(AnalysisSettings&&) = default;
AnalysisSettings& AnalysisSettings::operator=(AnalysisSettings&&) = default;
AnalysisSettings::~AnalysisSettings() = default;

ReportingSettings::ReportingSettings() = default;
ReportingSettings::ReportingSettings(GURL url,
                                     const std::string& dm_token,
                                     bool per_profile)
    : reporting_url(url), dm_token(dm_token), per_profile(per_profile) {}
ReportingSettings::ReportingSettings(ReportingSettings&&) = default;
ReportingSettings& ReportingSettings::operator=(ReportingSettings&&) = default;
ReportingSettings::~ReportingSettings() = default;

FileSystemSettings::FileSystemSettings() = default;
FileSystemSettings::FileSystemSettings(const FileSystemSettings&) = default;
FileSystemSettings::FileSystemSettings(FileSystemSettings&&) = default;
FileSystemSettings& FileSystemSettings::operator=(const FileSystemSettings&) =
    default;
FileSystemSettings& FileSystemSettings::operator=(FileSystemSettings&&) =
    default;
FileSystemSettings::~FileSystemSettings() = default;

const char* ConnectorPref(AnalysisConnector connector) {
  switch (connector) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      return kOnBulkDataEntryPref;
    case AnalysisConnector::FILE_DOWNLOADED:
      return kOnFileDownloadedPref;
    case AnalysisConnector::FILE_ATTACHED:
      return kOnFileAttachedPref;
    case AnalysisConnector::PRINT:
      return kOnPrintPref;
    case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
      NOTREACHED() << "Using unspecified analysis connector";
      return "";
  }
}

const char* ConnectorPref(ReportingConnector connector) {
  switch (connector) {
    case ReportingConnector::SECURITY_EVENT:
      return kOnSecurityEventPref;
  }
}

const char* ConnectorPref(FileSystemConnector connector) {
  switch (connector) {
    case FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD:
      return kSendDownloadToCloudPref;
  }
}

const char* ConnectorScopePref(AnalysisConnector connector) {
  switch (connector) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      return kOnBulkDataEntryScopePref;
    case AnalysisConnector::FILE_DOWNLOADED:
      return kOnFileDownloadedScopePref;
    case AnalysisConnector::FILE_ATTACHED:
      return kOnFileAttachedScopePref;
    case AnalysisConnector::PRINT:
      return kOnPrintScopePref;
    case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
      NOTREACHED() << "Using unspecified analysis connector";
      return "";
  }
}

const char* ConnectorScopePref(ReportingConnector connector) {
  switch (connector) {
    case ReportingConnector::SECURITY_EVENT:
      return kOnSecurityEventScopePref;
  }
}

TriggeredRule::Action GetHighestPrecedenceAction(
    const ContentAnalysisResponse& response,
    std::string* tag) {
  auto action = TriggeredRule::ACTION_UNSPECIFIED;

  for (const auto& result : response.results()) {
    if (!result.has_status() ||
        result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    for (const auto& rule : result.triggered_rules()) {
      auto higher_precedence_action =
          GetHighestPrecedenceAction(action, rule.action());
      if (higher_precedence_action != action && tag != nullptr) {
        *tag = result.tag();
      }
      action = higher_precedence_action;
    }
  }
  return action;
}

TriggeredRule::Action GetHighestPrecedenceAction(
    const TriggeredRule::Action& action_1,
    const TriggeredRule::Action& action_2) {
  // Don't use the enum's int values to determine precedence since that
  // may introduce bugs for new actions later.
  //
  // The current precedence is BLOCK > WARN > REPORT_ONLY > UNSPECIFIED
  if (action_1 == TriggeredRule::BLOCK || action_2 == TriggeredRule::BLOCK) {
    return TriggeredRule::BLOCK;
  }
  if (action_1 == TriggeredRule::WARN || action_2 == TriggeredRule::WARN) {
    return TriggeredRule::WARN;
  }
  if (action_1 == TriggeredRule::REPORT_ONLY ||
      action_2 == TriggeredRule::REPORT_ONLY) {
    return TriggeredRule::REPORT_ONLY;
  }
  if (action_1 == TriggeredRule::ACTION_UNSPECIFIED ||
      action_2 == TriggeredRule::ACTION_UNSPECIFIED) {
    return TriggeredRule::ACTION_UNSPECIFIED;
  }
  NOTREACHED();
  return TriggeredRule::ACTION_UNSPECIFIED;
}

FileMetadata::FileMetadata(const std::string& filename,
                           const std::string& sha256,
                           const std::string& mime_type,
                           int64_t size,
                           const ContentAnalysisResponse& scan_response)
    : filename(filename),
      sha256(sha256),
      mime_type(mime_type),
      size(size),
      scan_response(scan_response) {}
FileMetadata::FileMetadata(FileMetadata&&) = default;
FileMetadata::FileMetadata(const FileMetadata&) = default;
FileMetadata& FileMetadata::operator=(const FileMetadata&) = default;
FileMetadata::~FileMetadata() = default;

const char ScanResult::kKey[] = "enterprise_connectors.scan_result_key";
ScanResult::ScanResult() = default;
ScanResult::ScanResult(FileMetadata metadata) {
  file_metadata.push_back(std::move(metadata));
}
ScanResult::~ScanResult() = default;

const char SavePackageScanningData::kKey[] =
    "enterprise_connectors.save_package_scanning_key";
SavePackageScanningData::SavePackageScanningData(
    content::SavePackageAllowedCallback callback)
    : callback(std::move(callback)) {}
SavePackageScanningData::~SavePackageScanningData() = default;

void RunSavePackageScanningCallback(download::DownloadItem* item,
                                    bool allowed) {
  DCHECK(item);

  auto* data = static_cast<SavePackageScanningData*>(
      item->GetUserData(SavePackageScanningData::kKey));
  if (data && !data->callback.is_null())
    std::move(data->callback).Run(allowed);
}

bool ContainsMalwareVerdict(const ContentAnalysisResponse& response) {
  const auto& results = response.results();
  return std::any_of(results.begin(), results.end(), [](const auto& result) {
    return result.tag() == kMalwareTag && !result.triggered_rules().empty();
  });
}

bool IncludeDeviceInfo(Profile* profile, bool per_profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return policy::PolicyLoaderLacros::IsMainUserAffiliated();
#else
  return !per_profile;
#endif
}

bool ShouldPromptReviewForDownload(Profile* profile,
                                   download::DownloadDangerType danger_type) {
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
      danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    return ConnectorsServiceFactory::GetForBrowserContext(profile)
        ->HasCustomInfoToDisplay(AnalysisConnector::FILE_DOWNLOADED, kDlpTag);
  } else if (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT) {
    return ConnectorsServiceFactory::GetForBrowserContext(profile)
        ->HasCustomInfoToDisplay(AnalysisConnector::FILE_DOWNLOADED,
                                 kMalwareTag);
  }
  return false;
}

void ShowDownloadReviewDialog(const std::u16string& filename,
                              Profile* profile,
                              download::DownloadItem* download_item,
                              content::WebContents* web_contents,
                              download::DownloadDangerType danger_type,
                              base::OnceClosure keep_closure,
                              base::OnceClosure discard_closure) {
  auto state = FinalContentAnalysisResult::FAILURE;
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
    state = FinalContentAnalysisResult::WARNING;
  }

  const char* tag =
      (danger_type ==
                   download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
               danger_type ==
                   download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK
           ? kDlpTag
           : kMalwareTag);

  auto* connectors_service =
      ConnectorsServiceFactory::GetForBrowserContext(profile);

  std::u16string custom_message =
      connectors_service
          ->GetCustomMessage(AnalysisConnector::FILE_DOWNLOADED, tag)
          .value_or(u"");
  GURL learn_more_url =
      connectors_service
          ->GetLearnMoreUrl(AnalysisConnector::FILE_DOWNLOADED, tag)
          .value_or(GURL());

  bool bypass_justification_required =
      connectors_service
          ->GetBypassJustificationRequired(AnalysisConnector::FILE_DOWNLOADED,
                                           tag)
          .value_or(false);

  // This dialog opens itself, and is thereafter owned by constrained window
  // code.
  new ContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          filename, custom_message, learn_more_url,
          bypass_justification_required, std::move(keep_closure),
          std::move(discard_closure), download_item),
      web_contents, safe_browsing::DeepScanAccessPoint::DOWNLOAD,
      /* file_count */ 1, state, download_item);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
Profile* GetMainProfileLacros() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(
      profiles, [](Profile* profile) { return profile->IsMainProfile(); });
  if (main_it == profiles.end())
    return nullptr;
  return *main_it;
}
#endif

}  // namespace enterprise_connectors
