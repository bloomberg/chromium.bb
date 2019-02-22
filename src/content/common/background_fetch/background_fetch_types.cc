// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_types.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace {

blink::mojom::SerializedBlobPtr CloneSerializedBlob(
    const blink::mojom::SerializedBlobPtr& blob) {
  if (!blob)
    return nullptr;
  blink::mojom::BlobPtr blob_ptr(std::move(blob->blob));
  blob_ptr->Clone(mojo::MakeRequest(&blob->blob));
  return blink::mojom::SerializedBlob::New(
      blob->uuid, blob->content_type, blob->size, blob_ptr.PassInterface());
}

}  // namespace

namespace content {

BackgroundFetchOptions::BackgroundFetchOptions() = default;

BackgroundFetchOptions::BackgroundFetchOptions(
    const BackgroundFetchOptions& other) = default;

BackgroundFetchOptions::~BackgroundFetchOptions() = default;

BackgroundFetchRegistration::BackgroundFetchRegistration()
    : result(blink::mojom::BackgroundFetchResult::UNSET),
      failure_reason(blink::mojom::BackgroundFetchFailureReason::NONE) {}

BackgroundFetchRegistration::BackgroundFetchRegistration(
    const std::string& developer_id,
    const std::string& unique_id,
    uint64_t upload_total,
    uint64_t uploaded,
    uint64_t download_total,
    uint64_t downloaded,
    blink::mojom::BackgroundFetchResult result,
    blink::mojom::BackgroundFetchFailureReason failure_reason)
    : developer_id(developer_id),
      unique_id(unique_id),
      upload_total(upload_total),
      uploaded(uploaded),
      download_total(download_total),
      downloaded(downloaded),
      result(result),
      failure_reason(failure_reason) {}

BackgroundFetchRegistration::BackgroundFetchRegistration(
    const BackgroundFetchRegistration& other) = default;

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

// static
blink::mojom::FetchAPIResponsePtr BackgroundFetchSettledFetch::CloneResponse(
    const blink::mojom::FetchAPIResponsePtr& response) {
  // TODO(https://crbug.com/876546): Replace this method with response.Clone()
  // if the associated bug is fixed.
  if (!response)
    return nullptr;
  return blink::mojom::FetchAPIResponse::New(
      response->url_list, response->status_code, response->status_text,
      response->response_type, response->headers,
      CloneSerializedBlob(response->blob), response->error,
      response->response_time, response->cache_storage_cache_name,
      response->cors_exposed_header_names, response->is_in_cache_storage,
      CloneSerializedBlob(response->side_data_blob));
}
BackgroundFetchSettledFetch::BackgroundFetchSettledFetch() = default;

BackgroundFetchSettledFetch::BackgroundFetchSettledFetch(
    const BackgroundFetchSettledFetch& other) {
  *this = other;
}

BackgroundFetchSettledFetch& BackgroundFetchSettledFetch::operator=(
    const BackgroundFetchSettledFetch& other) {
  request = other.request;
  response = CloneResponse(other.response);
  return *this;
}

BackgroundFetchSettledFetch::~BackgroundFetchSettledFetch() = default;

}  // namespace content
