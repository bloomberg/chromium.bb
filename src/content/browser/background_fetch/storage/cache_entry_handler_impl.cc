// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/cache_entry_handler_impl.h"

#include "base/guid.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace content {
namespace background_fetch {

CacheEntryHandlerImpl::CacheEntryHandlerImpl(
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : CacheStorageCacheEntryHandler(std::move(blob_context)),
      weak_ptr_factory_(this) {}

CacheEntryHandlerImpl::~CacheEntryHandlerImpl() = default;

std::unique_ptr<PutContext> CacheEntryHandlerImpl::CreatePutContext(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchAPIResponsePtr response) {
  blink::mojom::BlobPtr response_blob;
  uint64_t response_blob_size = blink::BlobUtils::kUnknownSize;
  blink::mojom::BlobPtr request_blob;
  uint64_t request_blob_size = blink::BlobUtils::kUnknownSize;

  if (response->blob) {
    response_blob.Bind(std::move(response->blob->blob));
    response_blob_size = response->blob->size;
  }
  if (request->blob) {
    request_blob.Bind(std::move(request->blob->blob));
    request_blob_size = request->blob->size;
  }

  return std::make_unique<PutContext>(
      std::move(request), std::move(response), std::move(response_blob),
      response_blob_size, std::move(request_blob), request_blob_size);
}

void CacheEntryHandlerImpl::PopulateBody(
    scoped_refptr<BlobDataHandle> data_handle,
    const blink::mojom::SerializedBlobPtr& blob,
    CacheStorageCache::EntryIndex index) {
  disk_cache::Entry* entry = data_handle->entry().get();
  DCHECK(entry);

  blob->size = entry->GetDataSize(index);
  blob->uuid = base::GenerateGUID();

  auto blob_data = std::make_unique<storage::BlobDataBuilder>(blob->uuid);
  blob_data->AppendDiskCacheEntry(std::move(data_handle), entry, index);

  auto blob_handle = blob_context_->AddFinishedBlob(std::move(blob_data));
  storage::BlobImpl::Create(std::move(blob_handle), MakeRequest(&blob->blob));
}

void CacheEntryHandlerImpl::PopulateResponseBody(
    scoped_refptr<BlobDataHandle> data_handle,
    blink::mojom::FetchAPIResponse* response) {
  response->blob = blink::mojom::SerializedBlob::New();
  PopulateBody(std::move(data_handle), response->blob,
               CacheStorageCache::INDEX_RESPONSE_BODY);
}

void CacheEntryHandlerImpl::PopulateRequestBody(
    scoped_refptr<BlobDataHandle> data_handle,
    blink::mojom::FetchAPIRequest* request) {
  if (!data_handle->entry() ||
      !data_handle->entry()->GetDataSize(CacheStorageCache::INDEX_SIDE_DATA)) {
    return;
  }

  request->blob = blink::mojom::SerializedBlob::New();
  PopulateBody(std::move(data_handle), request->blob,
               CacheStorageCache::INDEX_SIDE_DATA);
}

base::WeakPtr<CacheStorageCacheEntryHandler>
CacheEntryHandlerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace background_fetch
}  // namespace content