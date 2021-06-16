// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/cef_crash_report_upload_thread.h"

#include "base/notreached.h"
#include "libcef/common/cef_crash_report_utils.h"
#include "third_party/crashpad/crashpad/client/settings.h"

using namespace crashpad;

CefCrashReportUploadThread::CefCrashReportUploadThread(
    CrashReportDatabase* database,
    const std::string& url,
    const Options& options,
    int max_uploads)
    : CrashReportUploadThread(database, url, options),
      max_uploads_(max_uploads) {}

CefCrashReportUploadThread::~CefCrashReportUploadThread() {}

void CefCrashReportUploadThread::ProcessPendingReports() {
  if (BackoffPending()) {
    // Try again later.
    return;
  }

  if (MaxUploadsEnabled()) {
    // Retrieve all completed reports.
    std::vector<CrashReportDatabase::Report> reports;
    if (database_->GetCompletedReports(&reports) !=
        CrashReportDatabase::kNoError) {
      // The database is sick. It might be prudent to stop trying to poke it
      // from this thread by abandoning the thread altogether. On the other
      // hand, if the problem is transient, it might be possible to talk to it
      // again on the next pass. For now, take the latter approach.
      return;
    }

    const time_t now = time(nullptr);
    const int kSeconds = 60 * 60 * 24;  // 24 hours

    // Count how many reports have completed in the last 24 hours.
    recent_upload_ct_ = 0;
    for (const CrashReportDatabase::Report& report : reports) {
      if (report.last_upload_attempt_time > now - kSeconds)
        recent_upload_ct_++;
    }
  }

  // Continue with processing pending reports.
  CrashReportUploadThread::ProcessPendingReports();
}

void CefCrashReportUploadThread::ProcessPendingReport(
    const CrashReportDatabase::Report& report) {
  // Always allow upload if it's been explicitly requested by the user.
  if (!report.upload_explicitly_requested) {
    if (!UploadsEnabled()) {
      // Don’t attempt an upload if there’s no URL or if uploads have been
      // disabled in the database’s settings.
      database_->SkipReportUpload(
          report.uuid, Metrics::CrashSkippedReason::kUploadsDisabled);
      return;
    }

    if (MaxUploadsExceeded()) {
      // Don't send uploads if the rate limit has been exceeded.
      database_->SkipReportUpload(
          report.uuid, Metrics::CrashSkippedReason::kUploadThrottled);
      return;
    }
  }

  if (BackoffPending()) {
    // Try again later.
    return;
  }

  std::unique_ptr<const CrashReportDatabase::UploadReport> upload_report;
  CrashReportDatabase::OperationStatus status =
      database_->GetReportForUploading(report.uuid, &upload_report);
  switch (status) {
    case CrashReportDatabase::kNoError:
      break;

    case CrashReportDatabase::kBusyError:
      return;

    case CrashReportDatabase::kReportNotFound:
    case CrashReportDatabase::kFileSystemError:
    case CrashReportDatabase::kDatabaseError:
      // In these cases, SkipReportUpload() might not work either, but it’s best
      // to at least try to get the report out of the way.
      database_->SkipReportUpload(report.uuid,
                                  Metrics::CrashSkippedReason::kDatabaseError);
      return;

    case CrashReportDatabase::kCannotRequestUpload:
      NOTREACHED();
      return;
  }

  std::string response_body;
  UploadResult upload_result =
      UploadReport(upload_report.get(), &response_body);
  switch (upload_result) {
    case UploadResult::kSuccess:
      // The upload completed successfully.
      database_->RecordUploadComplete(std::move(upload_report), response_body);
      if (MaxUploadsEnabled())
        recent_upload_ct_++;
      ResetBackoff();
      break;
    case UploadResult::kPermanentFailure:
      // The upload should never be retried.
      database_->SkipReportUpload(report.uuid,
                                  Metrics::CrashSkippedReason::kUploadFailed);
      break;
    case UploadResult::kRetry:
      // The upload will be retried after a reasonable backoff delay. Since we
      // didn't successfully upload it we won't count it against the rate limit.
      IncreaseBackoff();
      break;
  }
}

CrashReportUploadThread::ParameterMap
CefCrashReportUploadThread::FilterParameters(const ParameterMap& parameters) {
  return crash_report_utils::FilterParameters(parameters);
}

bool CefCrashReportUploadThread::UploadsEnabled() const {
  Settings* const settings = database_->GetSettings();
  bool uploads_enabled;
  return !url_.empty() && settings->GetUploadsEnabled(&uploads_enabled) &&
         uploads_enabled;
}

bool CefCrashReportUploadThread::MaxUploadsEnabled() const {
  return options_.rate_limit && max_uploads_ > 0;
}

bool CefCrashReportUploadThread::MaxUploadsExceeded() const {
  return MaxUploadsEnabled() && recent_upload_ct_ >= max_uploads_;
}

bool CefCrashReportUploadThread::BackoffPending() const {
  if (!options_.rate_limit)
    return false;

  Settings* const settings = database_->GetSettings();

  time_t next_upload_time;
  if (settings->GetNextUploadAttemptTime(&next_upload_time) &&
      next_upload_time > 0) {
    const time_t now = time(nullptr);
    if (now < next_upload_time)
      return true;
  }

  return false;
}

void CefCrashReportUploadThread::IncreaseBackoff() {
  if (!options_.rate_limit)
    return;

  const int kHour = 60 * 60;  // 1 hour
  const int kBackoffSchedule[] = {
      kHour / 4,   // 15 minutes
      kHour,       // 1 hour
      kHour * 2,   // 2 hours
      kHour * 4,   // 4 hours
      kHour * 8,   // 8 hours
      kHour * 24,  // 24 hours
  };
  const int kBackoffScheduleSize =
      sizeof(kBackoffSchedule) / sizeof(kBackoffSchedule[0]);

  Settings* settings = database_->GetSettings();

  int backoff_step = 0;
  if (settings->GetBackoffStep(&backoff_step) && backoff_step < 0)
    backoff_step = 0;
  if (++backoff_step > kBackoffScheduleSize)
    backoff_step = kBackoffScheduleSize;

  time_t next_upload_time = time(nullptr);  // now
  next_upload_time += kBackoffSchedule[backoff_step - 1];

  settings->SetBackoffStep(backoff_step);
  settings->SetNextUploadAttemptTime(next_upload_time);

  if (max_uploads_ > 1) {
    // If the server is having trouble then we don't want to send many crash
    // reports after the backoff expires. Reduce max uploads to 1 per 24 hours
    // until the client is restarted.
    max_uploads_ = 1;
  }
}

void CefCrashReportUploadThread::ResetBackoff() {
  if (!options_.rate_limit)
    return;

  Settings* settings = database_->GetSettings();
  settings->SetBackoffStep(0);
  settings->SetNextUploadAttemptTime(0);
}
