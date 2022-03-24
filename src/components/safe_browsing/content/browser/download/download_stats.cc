// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/download/download_stats.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/safe_browsing/content/common/file_type_policies.h"

namespace {

using safe_browsing::FileTypePolicies;

constexpr char kShownMetricSuffix[] = ".Shown";
constexpr char kBypassedMetricSuffix[] = ".Bypassed";

std::string GetDangerTypeMetricSuffix(
    download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return ".DangerousFileType";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return ".Malicious";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return ".Uncommon";
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_UNSUPPORTED_FILETYPE:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      return ".Others";
  }
}

void RecordDangerousWarningFileType(download::DownloadDangerType danger_type,
                                    const base::FilePath& file_path,
                                    const std::string& metrics_suffix) {
  base::UmaHistogramSparse(
      "SBClientDownload.Warning.FileType" +
          GetDangerTypeMetricSuffix(danger_type) + metrics_suffix,
      FileTypePolicies::GetInstance()->UmaValueForFile(file_path));
}

void RecordDangerousWarningIsHttps(download::DownloadDangerType danger_type,
                                   bool is_https,
                                   const std::string& metrics_suffix) {
  base::UmaHistogramBoolean("SBClientDownload.Warning.DownloadIsHttps" +
                                GetDangerTypeMetricSuffix(danger_type) +
                                metrics_suffix,
                            is_https);
}

void RecordDangerousWarningHasUserGesture(
    download::DownloadDangerType danger_type,
    bool has_user_gesture,
    const std::string& metrics_suffix) {
  base::UmaHistogramBoolean("SBClientDownload.Warning.DownloadHasUserGesture" +
                                GetDangerTypeMetricSuffix(danger_type) +
                                metrics_suffix,
                            has_user_gesture);
}

}  // namespace

namespace safe_browsing {

void RecordDangerousDownloadWarningShown(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture) {
  RecordDangerousWarningFileType(danger_type, file_path, kShownMetricSuffix);
  RecordDangerousWarningIsHttps(danger_type, is_https, kShownMetricSuffix);
  RecordDangerousWarningHasUserGesture(danger_type, has_user_gesture,
                                       kShownMetricSuffix);
  base::RecordAction(
      base::UserMetricsAction("SafeBrowsing.Download.WarningShown"));
}

void RecordDangerousDownloadWarningBypassed(
    download::DownloadDangerType danger_type,
    const base::FilePath& file_path,
    bool is_https,
    bool has_user_gesture) {
  RecordDangerousWarningFileType(danger_type, file_path, kBypassedMetricSuffix);
  RecordDangerousWarningIsHttps(danger_type, is_https, kBypassedMetricSuffix);
  RecordDangerousWarningHasUserGesture(danger_type, has_user_gesture,
                                       kBypassedMetricSuffix);
  base::RecordAction(
      base::UserMetricsAction("SafeBrowsing.Download.WarningBypassed"));
}

void RecordDownloadOpened(download::DownloadDangerType danger_type,
                          download::DownloadContent download_content,
                          base::Time download_opened_time,
                          base::Time download_end_time,
                          bool show_download_in_folder) {
  if (danger_type != download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
    return;
  }
  std::string metric_suffix =
      show_download_in_folder ? ".ShowInFolder" : ".OpenDirectly";
  base::UmaHistogramCustomTimes(
      "SBClientDownload.SafeDownloadOpenedLatency2" + metric_suffix,
      /* sample */ download_opened_time - download_end_time,
      /* min */ base::Seconds(1),
      /* max */ base::Days(1), /* buckets */ 50);

  RecordDownloadOpenedFileType(download_content, download_opened_time,
                               download_end_time);
}

void RecordDownloadOpenedFileType(download::DownloadContent download_content,
                                  base::Time download_opened_time,
                                  base::Time download_end_time) {
  std::string download_content_str;
  switch (download_content) {
    case download::DownloadContent::UNRECOGNIZED:
      download_content_str = "UNRECOGNIZED";
      break;
    case download::DownloadContent::TEXT:
      download_content_str = "TEXT";
      break;
    case download::DownloadContent::IMAGE:
      download_content_str = "IMAGE";
      break;
    case download::DownloadContent::AUDIO:
      download_content_str = "AUDIO";
      break;
    case download::DownloadContent::VIDEO:
      download_content_str = "VIDEO";
      break;
    case download::DownloadContent::OCTET_STREAM:
      download_content_str = "OCTET_STREAM";
      break;
    case download::DownloadContent::PDF:
      download_content_str = "PDF";
      break;
    case download::DownloadContent::DOCUMENT:
      download_content_str = "DOCUMENT";
      break;
    case download::DownloadContent::SPREADSHEET:
      download_content_str = "SPREADSHEET";
      break;
    case download::DownloadContent::PRESENTATION:
      download_content_str = "PRESENTATION";
      break;
    case download::DownloadContent::ARCHIVE:
      download_content_str = "ARCHIVE";
      break;
    case download::DownloadContent::EXECUTABLE:
      download_content_str = "EXECUTABLE";
      break;
    case download::DownloadContent::DMG:
      download_content_str = "DMG";
      break;
    case download::DownloadContent::CRX:
      download_content_str = "CRX";
      break;
    case download::DownloadContent::WEB:
      download_content_str = "WEB";
      break;
    case download::DownloadContent::EBOOK:
      download_content_str = "EBOOK";
      break;
    case download::DownloadContent::FONT:
      download_content_str = "FONT";
      break;
    case download::DownloadContent::APK:
      download_content_str = "APK";
      break;
    case download::DownloadContent::MAX:
      download_content_str = "MAX";
      break;
  }
  base::UmaHistogramCustomTimes(
      "SBClientDownload.SafeDownloadOpenedLatencyByContentType." +
          download_content_str,
      /* sample */ download_opened_time - download_end_time,
      /* min */ base::Seconds(1),
      /* max */ base::Days(1), /* buckets */ 50);
}

void RecordDownloadFileTypeAttributes(
    DownloadFileType::DangerLevel danger_level,
    bool has_user_gesture,
    bool visited_referrer_before,
    absl::optional<base::Time> last_bypass_time) {
  if (danger_level != DownloadFileType::ALLOW_ON_USER_GESTURE) {
    return;
  }
  base::UmaHistogramEnumeration(
      "SBClientDownload.UserGestureFileType.Attributes",
      UserGestureFileTypeAttributes::TOTAL_TYPE_CHECKED);
  if (has_user_gesture) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::HAS_USER_GESTURE);
  }
  if (visited_referrer_before) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::HAS_REFERRER_VISIT);
  }
  if (has_user_gesture && visited_referrer_before) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::
            HAS_BOTH_USER_GESTURE_AND_REFERRER_VISIT);
  }
  if (last_bypass_time) {
    base::UmaHistogramEnumeration(
        "SBClientDownload.UserGestureFileType.Attributes",
        UserGestureFileTypeAttributes::HAS_BYPASSED_DOWNLOAD_WARNING);
    base::UmaHistogramCustomTimes(
        "SBClientDownload.UserGestureFileType.LastBypassDownloadInterval",
        /* sample */ base::Time::Now() - last_bypass_time.value(),
        /* min */ base::Seconds(1),
        /* max */ base::Days(1), /* buckets */ 50);
  }
}

}  // namespace safe_browsing
