// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper_delegate.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#include "ios/chrome/browser/download/mime_type_util.h"
#import "ios/web/public/download/download_task.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kIOSDownloadARModelStateHistogram[] =
    "Download.IOSDownloadARModelState";

const char kUsdzMimeTypeHistogramSuffix[] = ".USDZ";

namespace {

// When an AR Quick Look URL contains this fragment, scaling the displayed
// image (e.g., by pinch-zooming) is disallowed. See
// https://developer.apple.com/videos/play/wwdc2019/612/
const char kContentScalingSearchKey[] = "allowsContentScaling";

// Returns a suffix for Download.IOSDownloadARModelState histogram for the
// |download_task|.
std::string GetMimeTypeSuffix(web::DownloadTask* download_task) {
  DCHECK(IsUsdzFileFormat(download_task->GetOriginalMimeType(),
                          download_task->GetSuggestedFilename()));
  return kUsdzMimeTypeHistogramSuffix;
}

// Returns an enum for Download.IOSDownloadARModelState histogram for the
// terminated |download_task|.
IOSDownloadARModelState GetHistogramEnum(web::DownloadTask* download_task) {
  DCHECK(download_task);
  if (download_task->GetState() == web::DownloadTask::State::kNotStarted) {
    return IOSDownloadARModelState::kCreated;
  }
  if (download_task->GetState() == web::DownloadTask::State::kInProgress) {
    return IOSDownloadARModelState::kStarted;
  }
  DCHECK(download_task->IsDone());
  if (!IsUsdzFileFormat(download_task->GetMimeType(),
                        download_task->GetSuggestedFilename())) {
    return IOSDownloadARModelState::kWrongMimeTypeFailure;
  }
  if (download_task->GetHttpCode() == 401 ||
      download_task->GetHttpCode() == 403) {
    return IOSDownloadARModelState::kUnauthorizedFailure;
  }
  if (download_task->GetErrorCode()) {
    return IOSDownloadARModelState::kOtherFailure;
  }
  return IOSDownloadARModelState::kSuccessful;
}

// Logs Download.IOSDownloadARModelState* histogram for the |download_task|.
void LogHistogram(web::DownloadTask* download_task) {
  DCHECK(download_task);
  base::UmaHistogramEnumeration(
      kIOSDownloadARModelStateHistogram + GetMimeTypeSuffix(download_task),
      GetHistogramEnum(download_task));
}

}  // namespace

ARQuickLookTabHelper::ARQuickLookTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
}

ARQuickLookTabHelper::~ARQuickLookTabHelper() {
  if (download_task_) {
    RemoveCurrentDownload();
  }
}

void ARQuickLookTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<ARQuickLookTabHelper>(web_state));
  }
}

void ARQuickLookTabHelper::Download(
    std::unique_ptr<web::DownloadTask> download_task) {
  DCHECK(download_task);
  LogHistogram(download_task.get());

  base::FilePath download_dir;
  if (!GetTempDownloadsDirectory(&download_dir)) {
    return;
  }

  if (download_task_) {
    RemoveCurrentDownload();
  }
  // Take ownership of |download_task| and start the download.
  download_task_ = std::move(download_task);
  download_task_->AddObserver(this);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::CreateDirectory, download_dir),
      base::BindOnce(&ARQuickLookTabHelper::DownloadWithDestinationDir,
                     AsWeakPtr(), download_dir, download_task_.get()));
}

void ARQuickLookTabHelper::DidFinishDownload() {
  DCHECK_EQ(download_task_->GetState(), web::DownloadTask::State::kComplete);
  // Inform the delegate only if the download has been successful.
  if (download_task_->GetHttpCode() == 401 ||
      download_task_->GetHttpCode() == 403 || download_task_->GetErrorCode() ||
      !IsUsdzFileFormat(download_task_->GetMimeType(),
                        download_task_->GetSuggestedFilename())) {
    return;
  }

  NSURL* fileURL = [NSURL
      fileURLWithPath:base::SysUTF8ToNSString(
                          download_task_->GetResponsePath().AsUTF8Unsafe())];

  // Sets fragment component as |search_url|'s query so QueryIterator can check
  // if content scaling is not allowed (allowsContentScaling = 0)
  url::Replacements<char> replacement;
  GURL search_url = download_task_->GetOriginalUrl();
  std::string ref = search_url.ref();
  url::Component query_component(0, ref.length());
  replacement.SetQuery(ref.c_str(), query_component);
  search_url = search_url.ReplaceComponents(replacement);

  bool allow_content_scaling = true;
  std::string key_value;
  net::GetValueForKeyInQuery(search_url, kContentScalingSearchKey, &key_value);
  if (key_value == "0") {
    allow_content_scaling = false;
  }

  [delegate_ ARQuickLookTabHelper:this
      didFinishDowloadingFileWithURL:fileURL
                allowsContentScaling:allow_content_scaling];
}

void ARQuickLookTabHelper::RemoveCurrentDownload() {
  download_task_->RemoveObserver(this);
  download_task_.reset();
}

void ARQuickLookTabHelper::DownloadWithDestinationDir(
    const base::FilePath& destination_dir,
    web::DownloadTask* download_task,
    bool directory_created) {
  // Return early if |download_task_| has changed.
  if (download_task != download_task_.get()) {
    return;
  }

  if (!directory_created) {
    RemoveCurrentDownload();
    return;
  }

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  std::u16string file_name = download_task_->GetSuggestedFilename();
  base::FilePath path = destination_dir.Append(base::UTF16ToUTF8(file_name));
  download_task->Start(path, web::DownloadTask::Destination::kToDisk);
  LogHistogram(download_task_.get());
}

void ARQuickLookTabHelper::OnDownloadUpdated(web::DownloadTask* download_task) {
  DCHECK_EQ(download_task, download_task_.get());

  switch (download_task_->GetState()) {
    case web::DownloadTask::State::kCancelled:
      LogHistogram(download_task_.get());
      RemoveCurrentDownload();
      break;
    case web::DownloadTask::State::kInProgress:
      // Do nothing. Histogram is already logged after the task was started.
      break;
    case web::DownloadTask::State::kComplete:
      LogHistogram(download_task_.get());
      DidFinishDownload();
      break;
    case web::DownloadTask::State::kNotStarted:
      NOTREACHED() << "Invalid state.";
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ARQuickLookTabHelper)
