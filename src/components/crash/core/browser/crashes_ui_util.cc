// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/browser/crashes_ui_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/upload_list/upload_list.h"

namespace crash_reporter {

const CrashesUILocalizedString kCrashesUILocalizedStrings[] = {
    {"bugLinkText", IDS_CRASH_BUG_LINK_LABEL},
    {"crashCaptureTimeFormat", IDS_CRASH_CAPTURE_TIME_FORMAT},
    {"crashCountFormat", IDS_CRASH_CRASH_COUNT_BANNER_FORMAT},
    {"crashStatus", IDS_CRASH_REPORT_STATUS},
    {"crashStatusNotUploaded", IDS_CRASH_REPORT_STATUS_NOT_UPLOADED},
    {"crashStatusPending", IDS_CRASH_REPORT_STATUS_PENDING},
    {"crashStatusPendingUserRequested",
     IDS_CRASH_REPORT_STATUS_PENDING_USER_REQUESTED},
    {"crashStatusUploaded", IDS_CRASH_REPORT_STATUS_UPLOADED},
    {"crashesTitle", IDS_CRASH_TITLE},
    {"disabledHeader", IDS_CRASH_DISABLED_HEADER},
    {"disabledMessage", IDS_CRASH_DISABLED_MESSAGE},
    {"fileSize", IDS_CRASH_REPORT_FILE_SIZE},
    {"localId", IDS_CRASH_REPORT_LOCAL_ID},
    {"noCrashesMessage", IDS_CRASH_NO_CRASHES_MESSAGE},
    {"showDeveloperDetails", IDS_CRASH_SHOW_DEVELOPER_DETAILS},
    {"uploadCrashesLinkText", IDS_CRASH_UPLOAD_MESSAGE},
    {"uploadId", IDS_CRASH_REPORT_UPLOADED_ID},
    {"uploadNowLinkText", IDS_CRASH_UPLOAD_NOW_LINK_TEXT},
    {"uploadTime", IDS_CRASH_REPORT_UPLOADED_TIME},
};

const size_t kCrashesUILocalizedStringsCount =
    base::size(kCrashesUILocalizedStrings);

const char kCrashesUICrashesJS[] = "crashes.js";
const char kCrashesUIRequestCrashList[] = "requestCrashList";
const char kCrashesUIRequestCrashUpload[] = "requestCrashUpload";
const char kCrashesUIShortProductName[] = "shortProductName";
const char kCrashesUIUpdateCrashList[] = "updateCrashList";
const char kCrashesUIRequestSingleCrashUpload[] = "requestSingleCrashUpload";

std::string UploadInfoStateAsString(UploadList::UploadInfo::State state) {
  switch (state) {
    case UploadList::UploadInfo::State::NotUploaded:
      return "not_uploaded";
    case UploadList::UploadInfo::State::Pending:
      return "pending";
    case UploadList::UploadInfo::State::Pending_UserRequested:
      return "pending_user_requested";
    case UploadList::UploadInfo::State::Uploaded:
      return "uploaded";
  }

  NOTREACHED();
  return "";
}

void UploadListToValue(UploadList* upload_list, base::ListValue* out_value) {
  std::vector<UploadList::UploadInfo> crashes;
  upload_list->GetUploads(50, &crashes);

  for (const auto& info : crashes) {
    std::unique_ptr<base::DictionaryValue> crash(new base::DictionaryValue());
    crash->SetString("id", info.upload_id);
    if (info.state == UploadList::UploadInfo::State::Uploaded) {
      crash->SetString("upload_time",
                       base::TimeFormatFriendlyDateAndTime(info.upload_time));
    }
    if (!info.capture_time.is_null()) {
      crash->SetString("capture_time",
                       base::TimeFormatFriendlyDateAndTime(info.capture_time));
    }
    crash->SetString("local_id", info.local_id);
    crash->SetString("state", UploadInfoStateAsString(info.state));
    crash->SetString("file_size", info.file_size);
    out_value->Append(std::move(crash));
  }
}

}  // namespace crash_reporter
