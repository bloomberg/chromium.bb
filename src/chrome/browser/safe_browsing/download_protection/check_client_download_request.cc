// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/dm_token_utils.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/file_type_policies.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

namespace {

// This function is called when the result of malware scanning is already known
// (via |reason|), but we still want to perform DLP scanning.
void MaybeOverrideDlpScanResult(DownloadCheckResultReason reason,
                                CheckDownloadRepeatingCallback callback,
                                DownloadCheckResult deep_scan_result) {
  if (reason == REASON_DOWNLOAD_DANGEROUS) {
    switch (deep_scan_result) {
      case DownloadCheckResult::UNKNOWN:
      case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
      case DownloadCheckResult::DEEP_SCANNED_SAFE:
        callback.Run(DownloadCheckResult::DANGEROUS);
        return;

      case DownloadCheckResult::ASYNC_SCANNING:
      case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
      case DownloadCheckResult::BLOCKED_TOO_LARGE:
      case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
      case DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE:
        callback.Run(deep_scan_result);
        return;

      case DownloadCheckResult::SAFE:
      case DownloadCheckResult::UNCOMMON:
      case DownloadCheckResult::DANGEROUS_HOST:
      case DownloadCheckResult::WHITELISTED_BY_POLICY:
      case DownloadCheckResult::PROMPT_FOR_SCANNING:
      case DownloadCheckResult::DANGEROUS:
      case DownloadCheckResult::POTENTIALLY_UNWANTED:
        NOTREACHED();
        return;
    }
  } else if (reason == REASON_WHITELISTED_URL) {
    callback.Run(deep_scan_result);
    return;
  }

  NOTREACHED();
}

}  // namespace

using content::BrowserThread;

CheckClientDownloadRequest::CheckClientDownloadRequest(
    download::DownloadItem* item,
    CheckDownloadRepeatingCallback callback,
    DownloadProtectionService* service,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : CheckClientDownloadRequestBase(
          item->GetURL(),
          item->GetTargetFilePath(),
          item->GetFullPath(),
          {item->GetTabUrl(), item->GetTabReferrerUrl()},
          item->GetMimeType(),
          item->GetHash(),
          content::DownloadItemUtils::GetBrowserContext(item),
          callback,
          service,
          std::move(database_manager),
          std::move(binary_feature_extractor)),
      item_(item),
      callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->AddObserver(this);
  DVLOG(2) << "Starting SafeBrowsing download check for: "
           << item_->DebugString(true);
}

void CheckClientDownloadRequest::OnDownloadDestroyed(
    download::DownloadItem* download) {
  FinishRequest(DownloadCheckResult::UNKNOWN, REASON_DOWNLOAD_DESTROYED);
}

void CheckClientDownloadRequest::OnDownloadUpdated(
    download::DownloadItem* download) {
  // Consider the scan bypassed if the user clicked "Open now" or "Cancel"
  // before scanning is done.
  if (download->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING &&
      (download->GetState() == download::DownloadItem::COMPLETE ||
       download->GetState() == download::DownloadItem::CANCELLED)) {
    RecordDeepScanMetrics(
        /*access_point=*/DeepScanAccessPoint::DOWNLOAD,
        /*duration=*/base::TimeTicks::Now() - upload_start_time_,
        /*total_size=*/item_->GetTotalBytes(),
        /*result=*/"BypassedByUser",
        /*failure=*/true);
  }
}

// static
bool CheckClientDownloadRequest::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path,
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  if (item.GetUrlChain().empty()) {
    *reason = REASON_EMPTY_URL_CHAIN;
    return false;
  }
  const GURL& final_url = item.GetUrlChain().back();
  if (!final_url.is_valid() || final_url.is_empty()) {
    *reason = REASON_INVALID_URL;
    return false;
  }
  if (!final_url.IsStandard() && !final_url.SchemeIsBlob() &&
      !final_url.SchemeIs(url::kDataScheme)) {
    *reason = REASON_UNSUPPORTED_URL_SCHEME;
    return false;
  }
  // TODO(crbug.com/814813): Remove duplicated counting of REMOTE_FILE
  // and LOCAL_FILE in SBClientDownload.UnsupportedScheme.*.
  if (final_url.SchemeIsFile()) {
    *reason = final_url.has_host() ? REASON_REMOTE_FILE : REASON_LOCAL_FILE;
    return false;
  }
  // This check should be last, so we know the earlier checks passed.
  if (!FileTypePolicies::GetInstance()->IsCheckedBinaryFile(target_path)) {
    *reason = REASON_NOT_BINARY_FILE;
    return false;
  }
  *type = download_type_util::GetDownloadType(target_path);
  return true;
}

CheckClientDownloadRequest::~CheckClientDownloadRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::IsSupportedDownload(
    DownloadCheckResultReason* reason,
    ClientDownloadRequest::DownloadType* type) {
  return IsSupportedDownload(*item_, item_->GetTargetFilePath(), reason, type);
}

content::BrowserContext* CheckClientDownloadRequest::GetBrowserContext() const {
  return content::DownloadItemUtils::GetBrowserContext(item_);
}

bool CheckClientDownloadRequest::IsCancelled() {
  return item_->GetState() == download::DownloadItem::CANCELLED;
}

void CheckClientDownloadRequest::PopulateRequest(
    ClientDownloadRequest* request) {
  request->mutable_digests()->set_sha256(item_->GetHash());
  request->set_length(item_->GetReceivedBytes());
  for (size_t i = 0; i < item_->GetUrlChain().size(); ++i) {
    ClientDownloadRequest::Resource* resource = request->add_resources();
    resource->set_url(SanitizeUrl(item_->GetUrlChain()[i]));
    if (i == item_->GetUrlChain().size() - 1) {
      // The last URL in the chain is the download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource->set_referrer(SanitizeUrl(item_->GetReferrerUrl()));
      DVLOG(2) << "dl url " << resource->url();
      if (!item_->GetRemoteAddress().empty()) {
        resource->set_remote_ip(item_->GetRemoteAddress());
        DVLOG(2) << "  dl url remote addr: " << resource->remote_ip();
      }
      DVLOG(2) << "dl referrer " << resource->referrer();
    } else {
      DVLOG(2) << "dl redirect " << i << " " << resource->url();
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }
  }

  request->set_user_initiated(item_->HasUserGesture());

  auto* referrer_chain_data = static_cast<ReferrerChainData*>(
      item_->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey));
  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
    UMA_HISTOGRAM_COUNTS_100(
        "SafeBrowsing.ReferrerURLChainSize.DownloadAttribution",
        referrer_chain_data->referrer_chain_length());
  }
}

base::WeakPtr<CheckClientDownloadRequestBase>
CheckClientDownloadRequest::GetWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CheckClientDownloadRequest::NotifySendRequest(
    const ClientDownloadRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service()->client_download_request_callbacks_.Notify(item_, request);
}

void CheckClientDownloadRequest::SetDownloadPingToken(
    const std::string& token) {
  DCHECK(!token.empty());
  DownloadProtectionService::SetDownloadPingToken(item_, token);
}

void CheckClientDownloadRequest::MaybeStorePingsForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    const std::string& request_data,
    const std::string& response_body) {
  DownloadFeedbackService::MaybeStorePingsForDownload(
      result, upload_requested, item_, request_data, response_body);
}

bool CheckClientDownloadRequest::ShouldUploadBinary(
    DownloadCheckResultReason reason) {
  // If the download was destroyed, we can't upload it.
  if (reason == REASON_DOWNLOAD_DESTROYED)
    return false;

  return DeepScanningRequest::ShouldUploadItemByPolicy(item_);
}

void CheckClientDownloadRequest::UploadBinary(
    DownloadCheckResultReason reason) {
  if (reason == REASON_DOWNLOAD_DANGEROUS || reason == REASON_WHITELISTED_URL) {
    service()->UploadForDeepScanning(
        item_,
        base::BindRepeating(&MaybeOverrideDlpScanResult, reason, callback_),
        DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        {DeepScanningRequest::DeepScanType::SCAN_DLP});
  } else {
    service()->UploadForDeepScanning(
        item_, callback_, DeepScanningRequest::DeepScanTrigger::TRIGGER_POLICY,
        DeepScanningRequest::AllScans());
  }
}

void CheckClientDownloadRequest::NotifyRequestFinished(
    DownloadCheckResult result,
    DownloadCheckResultReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weakptr_factory_.InvalidateWeakPtrs();

  DVLOG(2) << "SafeBrowsing download verdict for: " << item_->DebugString(true)
           << " verdict:" << reason << " result:" << static_cast<int>(result);

  item_->RemoveObserver(this);
}

bool CheckClientDownloadRequest::ShouldPromptForDeepScanning(
    DownloadCheckResultReason reason) const {
  if (reason != REASON_DOWNLOAD_UNCOMMON)
    return false;

#if defined(OS_CHROMEOS)
  return false;
#else
  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  return base::FeatureList::IsEnabled(kPromptAppForDeepScanning) &&
         AdvancedProtectionStatusManagerFactory::GetForProfile(profile)
             ->IsUnderAdvancedProtection();
#endif
}

bool CheckClientDownloadRequest::IsWhitelistedByPolicy() const {
  Profile* profile = Profile::FromBrowserContext(GetBrowserContext());
  if (!profile)
    return false;
  return MatchesEnterpriseWhitelist(*profile->GetPrefs(), item_->GetUrlChain());
}

}  // namespace safe_browsing
