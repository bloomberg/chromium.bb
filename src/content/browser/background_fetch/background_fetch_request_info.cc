// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_response_headers.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    int request_index,
    blink::mojom::FetchAPIRequestPtr fetch_request,
    uint64_t request_body_size)
    : RefCountedDeleteOnSequence<BackgroundFetchRequestInfo>(
          base::SequencedTaskRunnerHandle::Get()),
      request_index_(request_index),
      fetch_request_(std::move(fetch_request)),
      request_body_size_(request_body_size) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchRequestInfo::InitializeDownloadGuid() {
  DCHECK(download_guid_.empty());

  download_guid_ = base::GenerateGUID();
}

void BackgroundFetchRequestInfo::SetDownloadGuid(
    const std::string& download_guid) {
  DCHECK(!download_guid.empty());
  DCHECK(download_guid_.empty());

  download_guid_ = download_guid;
}

void BackgroundFetchRequestInfo::SetResult(
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result);

  result_ = std::move(result);
  // The BackgroundFetchResponse was extracted when the download started.
  // This is sent over again when the download was complete in case the
  // browser was restarted.
  if (response_headers_.empty())
    PopulateWithResponse(std::move(result_->response));
  else
    result_->response.reset();

  // Get the response size.
  if (result_->blob_handle)
    response_size_ = result_->blob_handle->size();
  else
    response_size_ = result_->file_size;
}

void BackgroundFetchRequestInfo::SetEmptyResultWithFailureReason(
    BackgroundFetchResult::FailureReason failure_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  result_ = std::make_unique<BackgroundFetchResult>(
      /* response= */ nullptr, base::Time::Now(), failure_reason);
}

void BackgroundFetchRequestInfo::PopulateWithResponse(
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(response);

  url_chain_ = response->url_chain;

  // |headers| can be null when the request fails.
  if (!response->headers)
    return;

  // The response code, text and headers all are stored in the
  // net::HttpResponseHeaders object, shared by the |download_item|.
  response_code_ = response->headers->response_code();
  response_text_ = response->headers->GetStatusText();

  size_t iter = 0;
  std::string name, value;
  while (response->headers->EnumerateHeaderLines(&iter, &name, &value))
    response_headers_[base::ToLowerASCII(name)] = value;
}

int BackgroundFetchRequestInfo::GetResponseCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_code_;
}

const std::string& BackgroundFetchRequestInfo::GetResponseText() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_text_;
}

const std::map<std::string, std::string>&
BackgroundFetchRequestInfo::GetResponseHeaders() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_headers_;
}

const std::vector<GURL>& BackgroundFetchRequestInfo::GetURLChain() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_chain_;
}

void BackgroundFetchRequestInfo::CreateResponseBlobDataHandle(
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  DCHECK(!blob_data_handle_);

  if (!result_->blob_handle && result_->file_path.empty())
    return;

  // In Incognito mode the |result_->blob_handle| will be populated.
  if (result_->blob_handle) {
    blob_data_handle_ =
        std::make_unique<storage::BlobDataHandle>(*result_->blob_handle);
    result_->blob_handle.reset();
    return;
  }

  // In a normal profile, |result_->file_path| and |result_->file_size| will be
  // populated.
  auto blob_builder =
      std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
  blob_builder->AppendFile(result_->file_path, /* offset= */ 0,
                           result_->file_size,
                           /* expected_modification_time= */ base::Time());

  blob_data_handle_ = GetBlobStorageContext(blob_storage_context)
                          ->AddFinishedBlob(std::move(blob_builder));
  DCHECK_EQ(response_size_, blob_data_handle_ ? blob_data_handle_->size() : 0);
}

storage::BlobDataHandle*
BackgroundFetchRequestInfo::GetResponseBlobDataHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return blob_data_handle_.get();
}

std::unique_ptr<storage::BlobDataHandle>
BackgroundFetchRequestInfo::TakeResponseBlobDataHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return std::move(blob_data_handle_);
}

uint64_t BackgroundFetchRequestInfo::GetResponseSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return response_size_;
}

const base::Time& BackgroundFetchRequestInfo::GetResponseTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->response_time;
}

bool BackgroundFetchRequestInfo::IsResultSuccess() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result_);
  return result_->failure_reason == BackgroundFetchResult::FailureReason::NONE;
}

}  // namespace content
