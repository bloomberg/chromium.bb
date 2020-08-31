// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/legacy/legacy_cache_storage_cache.h"

#include <stddef.h>
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_entry_handler.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_cache_observer.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/browser/cache_storage/cache_storage_trace_utils.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/common/referrer_type_converters.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/padding_key.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using blink::mojom::CacheStorageError;
using blink::mojom::CacheStorageVerboseError;

namespace content {

namespace {

using ResponseHeaderMap = base::flat_map<std::string, std::string>;

const size_t kMaxQueryCacheResultBytes =
    1024 * 1024 * 10;  // 10MB query cache limit

// If the way that a cache's padding is calculated changes increment this
// version.
//
// History:
//
//   1: Uniform random 400K.
//   2: Uniform random 14,431K.
const int32_t kCachePaddingAlgorithmVersion = 2;

// Maximum number of recursive QueryCacheOpenNextEntry() calls we permit
// before forcing an asynchronous task.
const int kMaxQueryCacheRecursiveDepth = 20;

using MetadataCallback =
    base::OnceCallback<void(std::unique_ptr<proto::CacheMetadata>)>;

network::mojom::FetchResponseType ProtoResponseTypeToFetchResponseType(
    proto::CacheResponse::ResponseType response_type) {
  switch (response_type) {
    case proto::CacheResponse::BASIC_TYPE:
      return network::mojom::FetchResponseType::kBasic;
    case proto::CacheResponse::CORS_TYPE:
      return network::mojom::FetchResponseType::kCors;
    case proto::CacheResponse::DEFAULT_TYPE:
      return network::mojom::FetchResponseType::kDefault;
    case proto::CacheResponse::ERROR_TYPE:
      return network::mojom::FetchResponseType::kError;
    case proto::CacheResponse::OPAQUE_TYPE:
      return network::mojom::FetchResponseType::kOpaque;
    case proto::CacheResponse::OPAQUE_REDIRECT_TYPE:
      return network::mojom::FetchResponseType::kOpaqueRedirect;
  }
  NOTREACHED();
  return network::mojom::FetchResponseType::kOpaque;
}

proto::CacheResponse::ResponseType FetchResponseTypeToProtoResponseType(
    network::mojom::FetchResponseType response_type) {
  switch (response_type) {
    case network::mojom::FetchResponseType::kBasic:
      return proto::CacheResponse::BASIC_TYPE;
    case network::mojom::FetchResponseType::kCors:
      return proto::CacheResponse::CORS_TYPE;
    case network::mojom::FetchResponseType::kDefault:
      return proto::CacheResponse::DEFAULT_TYPE;
    case network::mojom::FetchResponseType::kError:
      return proto::CacheResponse::ERROR_TYPE;
    case network::mojom::FetchResponseType::kOpaque:
      return proto::CacheResponse::OPAQUE_TYPE;
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return proto::CacheResponse::OPAQUE_REDIRECT_TYPE;
  }
  NOTREACHED();
  return proto::CacheResponse::OPAQUE_TYPE;
}

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadMetadata(disk_cache::Entry* entry, MetadataCallback callback);
void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 MetadataCallback callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv);

bool VaryMatches(const blink::FetchAPIRequestHeadersMap& request,
                 const blink::FetchAPIRequestHeadersMap& cached_request,
                 const ResponseHeaderMap& response) {
  auto vary_iter = std::find_if(
      response.begin(), response.end(),
      [](const ResponseHeaderMap::value_type& pair) -> bool {
        return base::CompareCaseInsensitiveASCII(pair.first, "vary") == 0;
      });
  if (vary_iter == response.end())
    return true;

  for (const std::string& trimmed :
       base::SplitString(vary_iter->second, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    if (trimmed == "*")
      return false;

    auto request_iter = request.find(trimmed);
    auto cached_request_iter = cached_request.find(trimmed);

    // If the header exists in one but not the other, no match.
    if ((request_iter == request.end()) !=
        (cached_request_iter == cached_request.end()))
      return false;

    // If the header exists in one, it exists in both. Verify that the values
    // are equal.
    if (request_iter != request.end() &&
        request_iter->second != cached_request_iter->second)
      return false;
  }

  return true;
}

// Check a batch operation list for duplicate entries.  A StackVector
// must be passed to store any resulting duplicate URL strings.  Returns
// true if any duplicates were found.
bool FindDuplicateOperations(
    const std::vector<blink::mojom::BatchOperationPtr>& operations,
    std::vector<std::string>* duplicate_url_list_out) {
  using blink::mojom::BatchOperation;
  DCHECK(duplicate_url_list_out);

  if (operations.size() < 2) {
    return false;
  }

  // Create a temporary sorted vector of the operations to support quickly
  // finding potentially duplicate entries.  Multiple entries may have the
  // same URL, but differ by VARY header, so a sorted list is easier to
  // work with than a map.
  //
  // Note, this will use 512 bytes of stack space on 64-bit devices.  The
  // static size attempts to accommodate most typical Cache.addAll() uses in
  // service worker install events while not blowing up the stack too much.
  base::StackVector<BatchOperation*, 64> sorted;
  sorted->reserve(operations.size());
  for (const auto& op : operations) {
    sorted->push_back(op.get());
  }
  std::sort(sorted->begin(), sorted->end(),
            [](BatchOperation* left, BatchOperation* right) {
              return left->request->url < right->request->url;
            });

  // Check each entry in the sorted vector for any duplicates.  Since the
  // list is sorted we only need to inspect the immediate neighbors that
  // have the same URL.  This results in an average complexity of O(n log n).
  // If the entire list has entries with the same URL and different VARY
  // headers then this devolves into O(n^2).
  for (auto outer = sorted->cbegin(); outer != sorted->cend(); ++outer) {
    const BatchOperation* outer_op = *outer;

    // Note, the spec checks CacheQueryOptions like ignoreSearch, etc, but
    // currently there is no way for script to trigger a batch operation with
    // multiple entries and non-default options.  The only exposed API that
    // supports multiple operations is addAll() and it does not allow options
    // to be passed.  Therefore we assume we do not need to take any options
    // into account here.
    DCHECK(!outer_op->match_options);

    // If this entry already matches a duplicate we found, then just skip
    // ahead to find any remaining duplicates.
    if (!duplicate_url_list_out->empty() &&
        outer_op->request->url.spec() == duplicate_url_list_out->back()) {
      continue;
    }

    for (auto inner = std::next(outer); inner != sorted->cend(); ++inner) {
      const BatchOperation* inner_op = *inner;
      // Since the list is sorted we can stop looking at neighbors after
      // the first different URL.
      if (outer_op->request->url != inner_op->request->url) {
        break;
      }

      // VaryMatches() is asymmetric since the operation depends on the VARY
      // header in the target response.  Since we only visit each pair of
      // entries once we need to perform the VaryMatches() call in both
      // directions.
      if (VaryMatches(outer_op->request->headers, inner_op->request->headers,
                      inner_op->response->headers) ||
          VaryMatches(outer_op->request->headers, inner_op->request->headers,
                      outer_op->response->headers)) {
        duplicate_url_list_out->push_back(inner_op->request->url.spec());
        break;
      }
    }
  }

  return !duplicate_url_list_out->empty();
}

GURL RemoveQueryParam(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

void ReadMetadata(disk_cache::Entry* entry, MetadataCallback callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(
          entry->GetDataSize(LegacyCacheStorageCache::INDEX_HEADERS));

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |callback| is not
  // copyable.
  net::CompletionRepeatingCallback read_header_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          ReadMetadataDidReadMetadata, entry, std::move(callback), buffer));

  int read_rv =
      entry->ReadData(LegacyCacheStorageCache::INDEX_HEADERS, 0, buffer.get(),
                      buffer->size(), read_header_callback);

  if (read_rv != net::ERR_IO_PENDING)
    std::move(read_header_callback).Run(read_rv);
}

void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 MetadataCallback callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv) {
  if (rv != buffer->size()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<proto::CacheMetadata> metadata(new proto::CacheMetadata());

  if (!metadata->ParseFromArray(buffer->data(), buffer->size())) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(metadata));
}

blink::mojom::FetchAPIRequestPtr CreateRequest(
    const proto::CacheMetadata& metadata,
    const GURL& request_url) {
  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = request_url;
  request->method = metadata.request().method();
  request->is_reload = false;
  request->referrer = blink::mojom::Referrer::New();
  request->headers = {};

  for (int i = 0; i < metadata.request().headers_size(); ++i) {
    const proto::CacheHeaderMap header = metadata.request().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    request->headers.insert(std::make_pair(header.name(), header.value()));
  }
  return request;
}

blink::mojom::FetchAPIResponsePtr CreateResponse(
    const proto::CacheMetadata& metadata,
    const std::string& cache_name) {
  // We no longer support Responses with only a single URL entry.  This field
  // was deprecated in M57.
  if (metadata.response().has_url())
    return nullptr;

  std::vector<GURL> url_list;
  url_list.reserve(metadata.response().url_list_size());
  for (int i = 0; i < metadata.response().url_list_size(); ++i)
    url_list.push_back(GURL(metadata.response().url_list(i)));

  ResponseHeaderMap headers;
  for (int i = 0; i < metadata.response().headers_size(); ++i) {
    const proto::CacheHeaderMap header = metadata.response().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    headers.insert(std::make_pair(header.name(), header.value()));
  }

  return blink::mojom::FetchAPIResponse::New(
      url_list, metadata.response().status_code(),
      metadata.response().status_text(),
      ProtoResponseTypeToFetchResponseType(metadata.response().response_type()),
      network::mojom::FetchResponseSource::kCacheStorage, headers,
      nullptr /* blob */, blink::mojom::ServiceWorkerResponseError::kUnknown,
      base::Time::FromInternalValue(metadata.response().response_time()),
      cache_name,
      std::vector<std::string>(
          metadata.response().cors_exposed_header_names().begin(),
          metadata.response().cors_exposed_header_names().end()),
      nullptr /* side_data_blob */, nullptr /* side_data_blob_for_cache_put */,
      network::mojom::ParsedHeaders::New(),
      metadata.response().loaded_with_credentials());
}

// The size of opaque (non-cors) resource responses are padded in order
// to obfuscate their actual size.
bool ShouldPadResponseType(network::mojom::FetchResponseType response_type,
                           bool has_urls) {
  switch (response_type) {
    case network::mojom::FetchResponseType::kBasic:
    case network::mojom::FetchResponseType::kCors:
    case network::mojom::FetchResponseType::kDefault:
    case network::mojom::FetchResponseType::kError:
      return false;
    case network::mojom::FetchResponseType::kOpaque:
    case network::mojom::FetchResponseType::kOpaqueRedirect:
      return has_urls;
  }
  NOTREACHED();
  return false;
}

bool ShouldPadResourceSize(const content::proto::CacheResponse* response) {
  return ShouldPadResponseType(
      ProtoResponseTypeToFetchResponseType(response->response_type()),
      response->url_list_size());
}

bool ShouldPadResourceSize(const blink::mojom::FetchAPIResponse& response) {
  return ShouldPadResponseType(response.response_type,
                               !response.url_list.empty());
}

int64_t CalculateResponsePaddingInternal(
    const ::content::proto::CacheResponse* response,
    const crypto::SymmetricKey* padding_key,
    int side_data_size) {
  DCHECK(ShouldPadResourceSize(response));
  DCHECK_GE(side_data_size, 0);
  const std::string& url = response->url_list(response->url_list_size() - 1);
  bool loaded_with_credentials = response->has_loaded_with_credentials() &&
                                 response->loaded_with_credentials();
  return storage::ComputeResponsePadding(url, padding_key, side_data_size > 0,
                                         loaded_with_credentials);
}

net::RequestPriority GetDiskCachePriority(
    CacheStorageSchedulerPriority priority) {
  return priority == CacheStorageSchedulerPriority::kHigh ? net::HIGHEST
                                                          : net::MEDIUM;
}

}  // namespace

struct LegacyCacheStorageCache::QueryCacheResult {
  explicit QueryCacheResult(base::Time entry_time) : entry_time(entry_time) {}

  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::FetchAPIResponsePtr response;
  disk_cache::ScopedEntryPtr entry;
  base::Time entry_time;
};

struct LegacyCacheStorageCache::QueryCacheContext {
  QueryCacheContext(blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::CacheQueryOptionsPtr options,
                    QueryCacheCallback callback,
                    QueryTypes query_types)
      : request(std::move(request)),
        options(std::move(options)),
        callback(std::move(callback)),
        query_types(query_types),
        matches(std::make_unique<QueryCacheResults>()) {}

  ~QueryCacheContext() = default;

  // Input to QueryCache
  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::CacheQueryOptionsPtr options;
  QueryCacheCallback callback;
  QueryTypes query_types = 0;
  size_t estimated_out_bytes = 0;

  // Iteration state
  std::unique_ptr<disk_cache::Backend::Iterator> backend_iterator;

  // Output of QueryCache
  std::unique_ptr<std::vector<QueryCacheResult>> matches;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryCacheContext);
};

// static
std::unique_ptr<LegacyCacheStorageCache>
LegacyCacheStorageCache::CreateMemoryCache(
    const url::Origin& origin,
    CacheStorageOwner owner,
    const std::string& cache_name,
    LegacyCacheStorage* cache_storage,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    std::unique_ptr<crypto::SymmetricKey> cache_padding_key) {
  LegacyCacheStorageCache* cache = new LegacyCacheStorageCache(
      origin, owner, cache_name, base::FilePath(), cache_storage,
      std::move(scheduler_task_runner), std::move(quota_manager_proxy),
      std::move(blob_storage_context), 0 /* cache_size */,
      0 /* cache_padding */, std::move(cache_padding_key));
  cache->SetObserver(cache_storage);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

// static
std::unique_ptr<LegacyCacheStorageCache>
LegacyCacheStorageCache::CreatePersistentCache(
    const url::Origin& origin,
    CacheStorageOwner owner,
    const std::string& cache_name,
    LegacyCacheStorage* cache_storage,
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    int64_t cache_size,
    int64_t cache_padding,
    std::unique_ptr<crypto::SymmetricKey> cache_padding_key) {
  LegacyCacheStorageCache* cache = new LegacyCacheStorageCache(
      origin, owner, cache_name, path, cache_storage,
      std::move(scheduler_task_runner), std::move(quota_manager_proxy),
      std::move(blob_storage_context), cache_size, cache_padding,
      std::move(cache_padding_key));
  cache->SetObserver(cache_storage);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

base::WeakPtr<LegacyCacheStorageCache> LegacyCacheStorageCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CacheStorageCacheHandle LegacyCacheStorageCache::CreateHandle() {
  return CacheStorageCacheHandle(weak_ptr_factory_.GetWeakPtr());
}

void LegacyCacheStorageCache::AddHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handle_ref_count_ += 1;
  // Reference the parent CacheStorage while the Cache is referenced.  Some
  // code may only directly reference the Cache and we don't want to let the
  // CacheStorage cleanup if it becomes unreferenced in these cases.
  if (handle_ref_count_ == 1 && cache_storage_)
    cache_storage_handle_ = cache_storage_->CreateHandle();
}

void LegacyCacheStorageCache::DropHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(handle_ref_count_, 0U);
  handle_ref_count_ -= 1;
  // Dropping the last reference may result in the parent CacheStorage
  // deleting itself or this Cache object.  Be careful not to touch the
  // `this` pointer in this method after the following code.
  if (handle_ref_count_ == 0 && cache_storage_) {
    CacheStorageHandle handle = std::move(cache_storage_handle_);
    cache_storage_->CacheUnreferenced(this);
  }
}

bool LegacyCacheStorageCache::IsUnreferenced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !handle_ref_count_;
}

void LegacyCacheStorageCache::Match(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    CacheStorageSchedulerPriority priority,
    int64_t trace_id,
    ResponseCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kMatchBackendClosed), nullptr);
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared, CacheStorageSchedulerOp::kMatch,
      priority,
      base::BindOnce(
          &LegacyCacheStorageCache::MatchImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(match_options), trace_id, priority,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::MatchAll(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    int64_t trace_id,
    ResponsesCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kMatchAllBackendClosed),
        std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kMatchAll,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::MatchAllImpl,
          weak_ptr_factory_.GetWeakPtr(), std::move(request),
          std::move(match_options), trace_id,
          CacheStorageSchedulerPriority::kNormal,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::WriteSideData(ErrorCallback callback,
                                            const GURL& url,
                                            base::Time expected_response_time,
                                            int64_t trace_id,
                                            scoped_refptr<net::IOBuffer> buffer,
                                            int buf_len) {
  if (backend_state_ == BACKEND_CLOSED) {
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeErrorStorage(ErrorStorageType::kWriteSideDataBackendClosed)));
    return;
  }

  // GetUsageAndQuota is called before entering a scheduled operation since it
  // can call Size, another scheduled operation.
  quota_manager_proxy_->GetUsageAndQuota(
      scheduler_task_runner_.get(), origin_,
      blink::mojom::StorageType::kTemporary,
      base::BindOnce(&LegacyCacheStorageCache::WriteSideDataDidGetQuota,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), url,
                     expected_response_time, trace_id, buffer, buf_len));
}

void LegacyCacheStorageCache::BatchOperation(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    int64_t trace_id,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback) {
  // This method may produce a warning message that should be returned in the
  // final VerboseErrorCallback.  A message may be present in both the failure
  // and success paths.
  base::Optional<std::string> message;

  if (backend_state_ == BACKEND_CLOSED) {
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(ErrorStorageType::kBatchBackendClosed),
                std::move(message))));
    return;
  }

  // From BatchCacheOperations:
  //
  //   https://w3c.github.io/ServiceWorker/#batch-cache-operations-algorithm
  //
  // "If the result of running Query Cache with operation’s request,
  //  operation’s options, and addedItems is not empty, throw an
  //  InvalidStateError DOMException."
  std::vector<std::string> duplicate_url_list;
  if (FindDuplicateOperations(operations, &duplicate_url_list)) {
    // If we found any duplicates we need to at least warn the user.  Format
    // the URL list into a comma-separated list.
    std::string url_list_string = base::JoinString(duplicate_url_list, ", ");

    // Place the duplicate list into an error message.
    message.emplace(
        base::StringPrintf("duplicate requests (%s)", url_list_string.c_str()));

    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       CacheStorageVerboseError::New(
                           CacheStorageError::kErrorDuplicateOperation,
                           std::move(message))));
    return;
  }

  // Estimate the required size of the put operations. The size of the deletes
  // is unknown and not considered.
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  base::CheckedNumeric<uint64_t> safe_side_data_size = 0;
  for (const auto& operation : operations) {
    if (operation->operation_type == blink::mojom::OperationType::kPut) {
      safe_space_required += CalculateRequiredSafeSpaceForPut(operation);
      safe_side_data_size +=
          (operation->response->side_data_blob_for_cache_put
               ? operation->response->side_data_blob_for_cache_put->size
               : 0);
    }
  }
  if (!safe_space_required.IsValid() || !safe_side_data_size.IsValid()) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     std::move(bad_message_callback));
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(ErrorStorageType::kBatchInvalidSpace),
                std::move(message))));
    return;
  }
  uint64_t space_required = safe_space_required.ValueOrDie();
  uint64_t side_data_size = safe_side_data_size.ValueOrDie();
  if (space_required || side_data_size) {
    // GetUsageAndQuota is called before entering a scheduled operation since it
    // can call Size, another scheduled operation. This is racy. The decision
    // to commit is made before the scheduled Put operation runs. By the time
    // Put runs, the cache might already be full and the origin will be larger
    // than it's supposed to be.
    quota_manager_proxy_->GetUsageAndQuota(
        scheduler_task_runner_.get(), origin_,
        blink::mojom::StorageType::kTemporary,
        base::BindOnce(&LegacyCacheStorageCache::BatchDidGetUsageAndQuota,
                       weak_ptr_factory_.GetWeakPtr(), std::move(operations),
                       trace_id, std::move(callback),
                       std::move(bad_message_callback), std::move(message),
                       space_required, side_data_size));
    return;
  }

  BatchDidGetUsageAndQuota(std::move(operations), trace_id, std::move(callback),
                           std::move(bad_message_callback), std::move(message),
                           0 /* space_required */, 0 /* side_data_size */,
                           blink::mojom::QuotaStatusCode::kOk, 0 /* usage */,
                           0 /* quota */);
}

void LegacyCacheStorageCache::BatchDidGetUsageAndQuota(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    int64_t trace_id,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback,
    base::Optional<std::string> message,
    uint64_t space_required,
    uint64_t side_data_size,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  TRACE_EVENT_WITH_FLOW1("CacheStorage",
                         "LegacyCacheStorageCache::BatchDidGetUsageAndQuota",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "operations", CacheStorageTracedValue(operations));
  base::CheckedNumeric<uint64_t> safe_space_required = space_required;
  base::CheckedNumeric<uint64_t> safe_space_required_with_side_data;
  safe_space_required += usage;
  safe_space_required_with_side_data = safe_space_required + side_data_size;
  if (!safe_space_required.IsValid() ||
      !safe_space_required_with_side_data.IsValid()) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     std::move(bad_message_callback));
    scheduler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            CacheStorageVerboseError::New(
                MakeErrorStorage(
                    ErrorStorageType::kBatchDidGetUsageAndQuotaInvalidSpace),
                std::move(message))));
    return;
  }
  if (status_code != blink::mojom::QuotaStatusCode::kOk ||
      safe_space_required.ValueOrDie() > quota) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CacheStorageVerboseError::New(
                                      CacheStorageError::kErrorQuotaExceeded,
                                      std::move(message))));
    return;
  }
  bool skip_side_data = safe_space_required_with_side_data.ValueOrDie() > quota;

  // The following relies on the guarantee that the RepeatingCallback returned
  // from AdaptCallbackForRepeating invokes the original callback on the first
  // invocation, and (critically) that subsequent invocations are ignored.
  // TODO(jsbell): Replace AdaptCallbackForRepeating with ...? crbug.com/730593
  auto callback_copy = base::AdaptCallbackForRepeating(std::move(callback));
  auto barrier_closure = base::BarrierClosure(
      operations.size(),
      base::BindOnce(&LegacyCacheStorageCache::BatchDidAllOperations,
                     weak_ptr_factory_.GetWeakPtr(), callback_copy, message,
                     trace_id));
  auto completion_callback = base::BindRepeating(
      &LegacyCacheStorageCache::BatchDidOneOperation,
      weak_ptr_factory_.GetWeakPtr(), std::move(barrier_closure),
      std::move(callback_copy), std::move(message), trace_id);

  // Operations may synchronously invoke |callback| which could release the
  // last reference to this instance. Hold a handle for the duration of this
  // loop. (Asynchronous tasks scheduled by the operations use weak ptrs which
  // will no-op automatically.)
  CacheStorageCacheHandle handle = CreateHandle();

  for (auto& operation : operations) {
    switch (operation->operation_type) {
      case blink::mojom::OperationType::kPut:
        if (skip_side_data) {
          operation->response->side_data_blob_for_cache_put = nullptr;
          Put(std::move(operation), trace_id, completion_callback);
        } else {
          Put(std::move(operation), trace_id, completion_callback);
        }
        break;
      case blink::mojom::OperationType::kDelete:
        DCHECK_EQ(1u, operations.size());
        Delete(std::move(operation), completion_callback);
        break;
      case blink::mojom::OperationType::kUndefined:
        NOTREACHED();
        // TODO(nhiroki): This should return "TypeError".
        // http://crbug.com/425505
        completion_callback.Run(MakeErrorStorage(
            ErrorStorageType::kBatchDidGetUsageAndQuotaUndefinedOp));
        break;
    }
  }
}

void LegacyCacheStorageCache::BatchDidOneOperation(
    base::OnceClosure completion_closure,
    VerboseErrorCallback error_callback,
    base::Optional<std::string> message,
    int64_t trace_id,
    CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::BatchDidOneOperation",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (error != CacheStorageError::kSuccess) {
    // This relies on |callback| being created by AdaptCallbackForRepeating
    // and ignoring anything but the first invocation.
    std::move(error_callback)
        .Run(CacheStorageVerboseError::New(error, std::move(message)));
  }

  std::move(completion_closure).Run();
}

void LegacyCacheStorageCache::BatchDidAllOperations(
    VerboseErrorCallback callback,
    base::Optional<std::string> message,
    int64_t trace_id) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::BatchDidAllOperations",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // This relies on |callback| being created by AdaptCallbackForRepeating
  // and ignoring anything but the first invocation.
  std::move(callback).Run(CacheStorageVerboseError::New(
      CacheStorageError::kSuccess, std::move(message)));
}

void LegacyCacheStorageCache::Keys(blink::mojom::FetchAPIRequestPtr request,
                                   blink::mojom::CacheQueryOptionsPtr options,
                                   int64_t trace_id,
                                   RequestsCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysBackendClosed), nullptr);
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared, CacheStorageSchedulerOp::kKeys,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::KeysImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(options), trace_id,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::Close(base::OnceClosure callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_)
      << "Was LegacyCacheStorageCache::Close() called twice?";

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kClose, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::CloseImpl, weak_ptr_factory_.GetWeakPtr(),
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::Size(SizeCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    // TODO(jkarlin): Delete caches that can't be initialized.
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), 0));
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared, CacheStorageSchedulerOp::kSize,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::GetSizeThenClose(SizeCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), 0));
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kSizeThenClose,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &LegacyCacheStorageCache::GetSizeThenCloseDidGetSize,
              weak_ptr_factory_.GetWeakPtr(),
              scheduler_->WrapCallbackToRunNext(id, std::move(callback)))));
}

void LegacyCacheStorageCache::SetObserver(CacheStorageCacheObserver* observer) {
  DCHECK((observer == nullptr) ^ (cache_observer_ == nullptr));
  cache_observer_ = observer;
}

// static
size_t LegacyCacheStorageCache::EstimatedStructSize(
    const blink::mojom::FetchAPIRequestPtr& request) {
  size_t size = sizeof(*request);
  size += request->url.spec().size();

  for (const auto& key_and_value : request->headers) {
    size += key_and_value.first.size();
    size += key_and_value.second.size();
  }

  return size;
}

LegacyCacheStorageCache::~LegacyCacheStorageCache() {
  quota_manager_proxy_->NotifyOriginNoLongerInUse(origin_);
}

void LegacyCacheStorageCache::SetSchedulerForTesting(
    std::unique_ptr<CacheStorageScheduler> scheduler) {
  DCHECK(!scheduler_->ScheduledOperations());
  scheduler_ = std::move(scheduler);
}

LegacyCacheStorageCache::LegacyCacheStorageCache(
    const url::Origin& origin,
    CacheStorageOwner owner,
    const std::string& cache_name,
    const base::FilePath& path,
    LegacyCacheStorage* cache_storage,
    scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
    int64_t cache_size,
    int64_t cache_padding,
    std::unique_ptr<crypto::SymmetricKey> cache_padding_key)
    : origin_(origin),
      owner_(owner),
      cache_name_(cache_name),
      path_(path),
      cache_storage_(cache_storage),
      scheduler_task_runner_(std::move(scheduler_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      scheduler_(new CacheStorageScheduler(CacheStorageSchedulerClient::kCache,
                                           scheduler_task_runner_)),
      cache_size_(cache_size),
      cache_padding_(cache_padding),
      cache_padding_key_(std::move(cache_padding_key)),
      max_query_size_bytes_(kMaxQueryCacheResultBytes),
      cache_observer_(nullptr),
      cache_entry_handler_(
          CacheStorageCacheEntryHandler::CreateCacheEntryHandler(
              owner,
              std::move(blob_storage_context))),
      memory_only_(path.empty()) {
  DCHECK(!origin_.opaque());
  DCHECK(quota_manager_proxy_.get());
  DCHECK(cache_padding_key_.get());

  if (cache_size_ != CacheStorage::kSizeUnknown &&
      cache_padding_ != CacheStorage::kSizeUnknown) {
    // The size of this cache has already been reported to the QuotaManager.
    last_reported_size_ = cache_size_ + cache_padding_;
  }

  quota_manager_proxy_->NotifyOriginInUse(origin_);
}

void LegacyCacheStorageCache::QueryCache(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    QueryTypes query_types,
    CacheStorageSchedulerPriority priority,
    QueryCacheCallback callback) {
  DCHECK_NE(
      QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_WITH_BODIES,
      query_types & (QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_WITH_BODIES));
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kQueryCacheBackendClosed), nullptr);
    return;
  }

  if (owner_ != CacheStorageOwner::kBackgroundFetch &&
      (!options || !options->ignore_method) && request &&
      !request->method.empty() && request->method != "GET") {
    std::move(callback).Run(CacheStorageError::kSuccess,
                            std::make_unique<QueryCacheResults>());
    return;
  }

  std::string request_url;
  if (request)
    request_url = request->url.spec();

  std::unique_ptr<QueryCacheContext> query_cache_context(
      new QueryCacheContext(std::move(request), std::move(options),
                            std::move(callback), query_types));
  if (query_cache_context->request &&
      !query_cache_context->request->url.is_empty() &&
      (!query_cache_context->options ||
       !query_cache_context->options->ignore_search)) {
    // There is no need to scan the entire backend, just open the exact
    // URL.

    // Create a callback that is copyable, even though it can only be called
    // once. BindRepeating() cannot be used directly because
    // |query_cache_context| is not copyable.
    auto open_entry_callback = base::AdaptCallbackForRepeating(base::BindOnce(
        &LegacyCacheStorageCache::QueryCacheDidOpenFastPath,
        weak_ptr_factory_.GetWeakPtr(), std::move(query_cache_context)));

    disk_cache::EntryResult result = backend_->OpenEntry(
        request_url, GetDiskCachePriority(priority), open_entry_callback);
    if (result.net_error() != net::ERR_IO_PENDING)
      std::move(open_entry_callback).Run(std::move(result));
    return;
  }

  query_cache_context->backend_iterator = backend_->CreateIterator();
  QueryCacheOpenNextEntry(std::move(query_cache_context));
}

void LegacyCacheStorageCache::QueryCacheDidOpenFastPath(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::EntryResult result) {
  if (result.net_error() != net::OK) {
    QueryCacheContext* results = query_cache_context.get();
    std::move(results->callback)
        .Run(CacheStorageError::kSuccess,
             std::move(query_cache_context->matches));
    return;
  }
  QueryCacheFilterEntry(std::move(query_cache_context), std::move(result));
}

void LegacyCacheStorageCache::QueryCacheOpenNextEntry(
    std::unique_ptr<QueryCacheContext> query_cache_context) {
  query_cache_recursive_depth_ += 1;
  auto cleanup = base::ScopedClosureRunner(base::BindOnce(
      [](CacheStorageCacheHandle handle) {
        LegacyCacheStorageCache* self = From(handle);
        if (!self)
          return;
        DCHECK_GT(self->query_cache_recursive_depth_, 0);
        self->query_cache_recursive_depth_ -= 1;
      },
      CreateHandle()));

  if (!query_cache_context->backend_iterator) {
    // Iteration is complete.
    std::sort(query_cache_context->matches->begin(),
              query_cache_context->matches->end(), QueryCacheResultCompare);

    std::move(query_cache_context->callback)
        .Run(CacheStorageError::kSuccess,
             std::move(query_cache_context->matches));
    return;
  }

  disk_cache::Backend::Iterator& iterator =
      *query_cache_context->backend_iterator;

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |query_cache_context| is
  // not copyable.
  auto open_entry_callback = base::AdaptCallbackForRepeating(base::BindOnce(
      &LegacyCacheStorageCache::QueryCacheFilterEntry,
      weak_ptr_factory_.GetWeakPtr(), std::move(query_cache_context)));

  disk_cache::EntryResult result = iterator.OpenNextEntry(open_entry_callback);

  if (result.net_error() == net::ERR_IO_PENDING)
    return;

  // In most cases we can immediately invoke the callback when there is no
  // pending IO.  We must be careful, however, to avoid blowing out the stack
  // when iterating a large cache.  Only invoke the callback synchronously
  // if we have not recursed past a threshold depth.
  if (query_cache_recursive_depth_ <= kMaxQueryCacheRecursiveDepth) {
    std::move(open_entry_callback).Run(std::move(result));
    return;
  }

  scheduler_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(open_entry_callback), std::move(result)));
}

void LegacyCacheStorageCache::QueryCacheFilterEntry(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::EntryResult result) {
  if (result.net_error() == net::ERR_FAILED) {
    // This is the indicator that iteration is complete.
    query_cache_context->backend_iterator.reset();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  if (result.net_error() < 0) {
    std::move(query_cache_context->callback)
        .Run(MakeErrorStorage(ErrorStorageType::kQueryCacheFilterEntryFailed),
             std::move(query_cache_context->matches));
    return;
  }

  disk_cache::ScopedEntryPtr entry(result.ReleaseEntry());

  if (backend_state_ == BACKEND_CLOSED) {
    std::move(query_cache_context->callback)
        .Run(CacheStorageError::kErrorNotFound,
             std::move(query_cache_context->matches));
    return;
  }

  if (query_cache_context->request &&
      !query_cache_context->request->url.is_empty()) {
    GURL requestURL = query_cache_context->request->url;
    GURL cachedURL = GURL(entry->GetKey());

    if (query_cache_context->options &&
        query_cache_context->options->ignore_search) {
      requestURL = RemoveQueryParam(requestURL);
      cachedURL = RemoveQueryParam(cachedURL);
    }

    if (cachedURL != requestURL) {
      QueryCacheOpenNextEntry(std::move(query_cache_context));
      return;
    }
  }

  disk_cache::Entry* entry_ptr = entry.get();
  ReadMetadata(
      entry_ptr,
      base::BindOnce(&LegacyCacheStorageCache::QueryCacheDidReadMetadata,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(query_cache_context), std::move(entry)));
}

void LegacyCacheStorageCache::QueryCacheDidReadMetadata(
    std::unique_ptr<QueryCacheContext> query_cache_context,
    disk_cache::ScopedEntryPtr entry,
    std::unique_ptr<proto::CacheMetadata> metadata) {
  if (!metadata) {
    entry->Doom();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  // If the entry was created before we started adding entry times, then
  // default to using the Response object's time for sorting purposes.
  int64_t entry_time = metadata->has_entry_time()
                           ? metadata->entry_time()
                           : metadata->response().response_time();

  query_cache_context->matches->push_back(
      QueryCacheResult(base::Time::FromInternalValue(entry_time)));
  QueryCacheResult* match = &query_cache_context->matches->back();
  match->request = CreateRequest(*metadata, GURL(entry->GetKey()));
  match->response = CreateResponse(*metadata, cache_name_);

  if (!match->response) {
    entry->Doom();
    query_cache_context->matches->pop_back();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  if (query_cache_context->request &&
      (!query_cache_context->options ||
       !query_cache_context->options->ignore_vary) &&
      !VaryMatches(query_cache_context->request->headers,
                   match->request->headers, match->response->headers)) {
    query_cache_context->matches->pop_back();
    QueryCacheOpenNextEntry(std::move(query_cache_context));
    return;
  }

  auto blob_entry = cache_entry_handler_->CreateDiskCacheBlobEntry(
      CreateHandle(), std::move(entry));

  if (query_cache_context->query_types & QUERY_CACHE_ENTRIES)
    match->entry = std::move(blob_entry->disk_cache_entry());

  if (query_cache_context->query_types & QUERY_CACHE_REQUESTS) {
    query_cache_context->estimated_out_bytes +=
        EstimatedStructSize(match->request);
    if (query_cache_context->estimated_out_bytes > max_query_size_bytes_) {
      std::move(query_cache_context->callback)
          .Run(CacheStorageError::kErrorQueryTooLarge, nullptr);
      return;
    }

    cache_entry_handler_->PopulateRequestBody(blob_entry, match->request.get());
  } else {
    match->request.reset();
  }

  if (query_cache_context->query_types & QUERY_CACHE_RESPONSES_WITH_BODIES) {
    query_cache_context->estimated_out_bytes +=
        EstimatedResponseSizeWithoutBlob(*match->response);
    if (query_cache_context->estimated_out_bytes > max_query_size_bytes_) {
      std::move(query_cache_context->callback)
          .Run(CacheStorageError::kErrorQueryTooLarge, nullptr);
      return;
    }
    if (blob_entry->disk_cache_entry()->GetDataSize(INDEX_RESPONSE_BODY) == 0) {
      QueryCacheOpenNextEntry(std::move(query_cache_context));
      return;
    }

    cache_entry_handler_->PopulateResponseBody(blob_entry,
                                               match->response.get());
  } else if (!(query_cache_context->query_types &
               QUERY_CACHE_RESPONSES_NO_BODIES)) {
    match->response.reset();
  }

  QueryCacheOpenNextEntry(std::move(query_cache_context));
}

// static
bool LegacyCacheStorageCache::QueryCacheResultCompare(
    const QueryCacheResult& lhs,
    const QueryCacheResult& rhs) {
  return lhs.entry_time < rhs.entry_time;
}

// static
size_t LegacyCacheStorageCache::EstimatedResponseSizeWithoutBlob(
    const blink::mojom::FetchAPIResponse& response) {
  size_t size = sizeof(blink::mojom::FetchAPIResponse);
  for (const auto& url : response.url_list)
    size += url.spec().size();
  size += response.status_text.size();
  if (response.cache_storage_cache_name)
    size += response.cache_storage_cache_name->size();
  for (const auto& key_and_value : response.headers) {
    size += key_and_value.first.size();
    size += key_and_value.second.size();
  }
  for (const auto& header : response.cors_exposed_header_names)
    size += header.size();
  return size;
}

// static
int64_t LegacyCacheStorageCache::CalculateResponsePadding(
    const blink::mojom::FetchAPIResponse& response,
    const crypto::SymmetricKey* padding_key,
    int side_data_size) {
  DCHECK_GE(side_data_size, 0);
  if (!ShouldPadResourceSize(response))
    return 0;
  return storage::ComputeResponsePadding(response.url_list.back().spec(),
                                         padding_key, side_data_size > 0,
                                         response.loaded_with_credentials);
}

// static
int32_t LegacyCacheStorageCache::GetResponsePaddingVersion() {
  return kCachePaddingAlgorithmVersion;
}

void LegacyCacheStorageCache::MatchImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    int64_t trace_id,
    CacheStorageSchedulerPriority priority,
    ResponseCallback callback) {
  MatchAllImpl(
      std::move(request), std::move(match_options), trace_id, priority,
      base::BindOnce(&LegacyCacheStorageCache::MatchDidMatchAll,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LegacyCacheStorageCache::MatchDidMatchAll(
    ResponseCallback callback,
    CacheStorageError match_all_error,
    std::vector<blink::mojom::FetchAPIResponsePtr> match_all_responses) {
  if (match_all_error != CacheStorageError::kSuccess) {
    std::move(callback).Run(match_all_error, nullptr);
    return;
  }

  if (match_all_responses.empty()) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound, nullptr);
    return;
  }

  std::move(callback).Run(CacheStorageError::kSuccess,
                          std::move(match_all_responses[0]));
}

void LegacyCacheStorageCache::MatchAllImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    CacheStorageSchedulerPriority priority,
    ResponsesCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2("CacheStorage",
                         "LegacyCacheStorageCache::MatchAllImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(request), "options",
                         CacheStorageTracedValue(options));
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kStorageMatchAllBackendClosed),
        std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(std::move(request), std::move(options),
             QUERY_CACHE_REQUESTS | QUERY_CACHE_RESPONSES_WITH_BODIES, priority,
             base::BindOnce(&LegacyCacheStorageCache::MatchAllDidQueryCache,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            trace_id));
}

void LegacyCacheStorageCache::MatchAllDidQueryCache(
    ResponsesCallback callback,
    int64_t trace_id,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::MatchAllDidQueryCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error,
                            std::vector<blink::mojom::FetchAPIResponsePtr>());
    return;
  }

  std::vector<blink::mojom::FetchAPIResponsePtr> out_responses;
  out_responses.reserve(query_cache_results->size());

  for (auto& result : *query_cache_results) {
    out_responses.push_back(std::move(result.response));
  }

  std::move(callback).Run(CacheStorageError::kSuccess,
                          std::move(out_responses));
}

void LegacyCacheStorageCache::WriteSideDataDidGetQuota(
    ErrorCallback callback,
    const GURL& url,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::WriteSideDataDidGetQuota",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (status_code != blink::mojom::QuotaStatusCode::kOk ||
      (buf_len > quota - usage)) {
    scheduler_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  CacheStorageError::kErrorQuotaExceeded));
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kWriteSideData,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&LegacyCacheStorageCache::WriteSideDataImpl,
                     weak_ptr_factory_.GetWeakPtr(),
                     scheduler_->WrapCallbackToRunNext(id, std::move(callback)),
                     url, expected_response_time, trace_id, buffer, buf_len));
}

void LegacyCacheStorageCache::WriteSideDataImpl(
    ErrorCallback callback,
    const GURL& url,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW1(
      "CacheStorage", "LegacyCacheStorageCache::WriteSideDataImpl",
      TRACE_ID_GLOBAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", url.spec());
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kWriteSideDataImplBackendClosed));
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |callback| is not copyable.
  auto open_entry_callback = base::AdaptCallbackForRepeating(
      base::BindOnce(&LegacyCacheStorageCache::WriteSideDataDidOpenEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     expected_response_time, trace_id, buffer, buf_len));

  // Note, the simple disk_cache priority is not important here because we
  // only allow one write operation at a time.  Therefore there will be no
  // competing operations in the disk_cache queue.
  disk_cache::EntryResult result =
      backend_->OpenEntry(url.spec(), net::MEDIUM, open_entry_callback);
  if (result.net_error() != net::ERR_IO_PENDING)
    std::move(open_entry_callback).Run(std::move(result));
}

void LegacyCacheStorageCache::WriteSideDataDidOpenEntry(
    ErrorCallback callback,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    disk_cache::EntryResult result) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::WriteSideDataDidOpenEntry",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (result.net_error() != net::OK) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound);
    return;
  }

  // Give the ownership of entry to a ScopedWritableEntry which will doom the
  // entry before closing unless we tell it that writing has successfully
  // completed via WritingCompleted.
  ScopedWritableEntry entry(result.ReleaseEntry());
  disk_cache::Entry* entry_ptr = entry.get();

  ReadMetadata(
      entry_ptr,
      base::BindOnce(&LegacyCacheStorageCache::WriteSideDataDidReadMetaData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     expected_response_time, trace_id, buffer, buf_len,
                     std::move(entry)));
}

void LegacyCacheStorageCache::WriteSideDataDidReadMetaData(
    ErrorCallback callback,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    ScopedWritableEntry entry,
    std::unique_ptr<proto::CacheMetadata> headers) {
  TRACE_EVENT_WITH_FLOW0(
      "CacheStorage", "LegacyCacheStorageCache::WriteSideDataDidReadMetaData",
      TRACE_ID_GLOBAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (!headers || headers->response().response_time() !=
                      expected_response_time.ToInternalValue()) {
    WriteSideDataComplete(std::move(callback), std::move(entry),
                          CacheStorageError::kErrorNotFound);
    return;
  }
  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = entry.get();

  std::unique_ptr<content::proto::CacheResponse> response(
      headers->release_response());

  int side_data_size_before_write = 0;
  if (ShouldPadResourceSize(response.get()))
    side_data_size_before_write = entry->GetDataSize(INDEX_SIDE_DATA);

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |callback|, |entry| and
  // |response| are not copyable.
  net::CompletionRepeatingCallback write_side_data_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          &LegacyCacheStorageCache::WriteSideDataDidWrite,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(entry),
          buf_len, std::move(response), side_data_size_before_write, trace_id));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  int rv = temp_entry_ptr->WriteData(
      INDEX_SIDE_DATA, 0 /* offset */, buffer.get(), buf_len,
      write_side_data_callback, true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    std::move(write_side_data_callback).Run(rv);
}

void LegacyCacheStorageCache::WriteSideDataDidWrite(
    ErrorCallback callback,
    ScopedWritableEntry entry,
    int expected_bytes,
    std::unique_ptr<::content::proto::CacheResponse> response,
    int side_data_size_before_write,
    int64_t trace_id,
    int rv) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::WriteSideDataDidWrite",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN);
  if (rv != expected_bytes) {
    WriteSideDataComplete(std::move(callback), std::move(entry),
                          CacheStorageError::kErrorStorage);
    return;
  }

  if (ShouldPadResourceSize(response.get())) {
    cache_padding_ -= CalculateResponsePaddingInternal(
        response.get(), cache_padding_key_.get(), side_data_size_before_write);

    cache_padding_ += CalculateResponsePaddingInternal(
        response.get(), cache_padding_key_.get(), rv);
  }

  WriteSideDataComplete(std::move(callback), std::move(entry),
                        CacheStorageError::kSuccess);
}

void LegacyCacheStorageCache::WriteSideDataComplete(
    ErrorCallback callback,
    ScopedWritableEntry entry,
    blink::mojom::CacheStorageError error) {
  if (error != CacheStorageError::kSuccess) {
    // If we found the entry, then we possibly wrote something and now we're
    // dooming the entry, causing a change in size, so update the size before
    // returning.
    if (error != CacheStorageError::kErrorNotFound) {
      UpdateCacheSize(base::BindOnce(std::move(callback), error));
      return;
    }

    entry.get_deleter()
        .WritingCompleted();  // Since we didn't change the entry.
    std::move(callback).Run(error);
    return;
  }

  entry.get_deleter().WritingCompleted();  // Since we didn't change the entry.
  UpdateCacheSize(base::BindOnce(std::move(callback), error));
}

void LegacyCacheStorageCache::Put(blink::mojom::BatchOperationPtr operation,
                                  int64_t trace_id,
                                  ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(blink::mojom::OperationType::kPut, operation->operation_type);
  Put(std::move(operation->request), std::move(operation->response), trace_id,
      std::move(callback));
}

void LegacyCacheStorageCache::Put(blink::mojom::FetchAPIRequestPtr request,
                                  blink::mojom::FetchAPIResponsePtr response,
                                  int64_t trace_id,
                                  ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);

  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.Cache.AllWritesResponseType",
                            response->response_type);

  auto put_context = cache_entry_handler_->CreatePutContext(
      std::move(request), std::move(response), trace_id);
  auto id = scheduler_->CreateId();
  put_context->callback =
      scheduler_->WrapCallbackToRunNext(id, std::move(callback));

  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive, CacheStorageSchedulerOp::kPut,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&LegacyCacheStorageCache::PutImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context)));
}

void LegacyCacheStorageCache::PutImpl(std::unique_ptr<PutContext> put_context) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2(
      "CacheStorage", "LegacyCacheStorageCache::PutImpl",
      TRACE_ID_GLOBAL(put_context->trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "request",
      CacheStorageTracedValue(put_context->request), "response",
      CacheStorageTracedValue(put_context->response));
  if (backend_state_ != BACKEND_OPEN) {
    PutComplete(std::move(put_context),
                MakeErrorStorage(ErrorStorageType::kPutImplBackendClosed));
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  put_context->callback =
      WrapCallbackWithHandle(std::move(put_context->callback));

  // Explicitly delete the incumbent resource (which may not exist). This is
  // only done so that it's padding will be decremented from the calculated
  // cache padding.
  // TODO(cmumford): Research alternatives to this explicit delete as it
  // seriously impacts put performance.
  auto delete_request = blink::mojom::FetchAPIRequest::New();
  delete_request->url = put_context->request->url;
  delete_request->method = "";
  delete_request->is_reload = false;
  delete_request->referrer = blink::mojom::Referrer::New();
  delete_request->headers = {};

  blink::mojom::CacheQueryOptionsPtr query_options =
      blink::mojom::CacheQueryOptions::New();
  query_options->ignore_method = true;
  query_options->ignore_vary = true;
  DeleteImpl(
      std::move(delete_request), std::move(query_options),
      base::BindOnce(&LegacyCacheStorageCache::PutDidDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context)));
}

void LegacyCacheStorageCache::PutDidDeleteEntry(
    std::unique_ptr<PutContext> put_context,
    CacheStorageError error) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::PutDidDeleteEntry",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (backend_state_ != BACKEND_OPEN) {
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kPutDidDeleteEntryBackendClosed));
    return;
  }

  if (error != CacheStorageError::kSuccess &&
      error != CacheStorageError::kErrorNotFound) {
    PutComplete(std::move(put_context), error);
    return;
  }

  const blink::mojom::FetchAPIRequest& request_ = *(put_context->request);
  disk_cache::Backend* backend_ptr = backend_.get();

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |put_context| is not
  // copyable.
  auto create_entry_callback = base::AdaptCallbackForRepeating(
      base::BindOnce(&LegacyCacheStorageCache::PutDidCreateEntry,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context)));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  disk_cache::EntryResult result = backend_ptr->OpenOrCreateEntry(
      request_.url.spec(), net::MEDIUM, create_entry_callback);

  if (result.net_error() != net::ERR_IO_PENDING)
    std::move(create_entry_callback).Run(std::move(result));
}

void LegacyCacheStorageCache::PutDidCreateEntry(
    std::unique_ptr<PutContext> put_context,
    disk_cache::EntryResult result) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::PutDidCreateEntry",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  int rv = result.net_error();

  // Moving the entry into a ScopedWritableEntry which will doom the entry
  // before closing unless we tell it that writing has successfully completed
  // via WritingCompleted.
  put_context->cache_entry.reset(result.ReleaseEntry());

  base::UmaHistogramSparse("ServiceWorkerCache.DiskCacheCreateEntryResult",
                           std::abs(rv));
  if (rv != net::OK) {
    quota_manager_proxy_->NotifyWriteFailed(origin_);
    PutComplete(std::move(put_context), CacheStorageError::kErrorExists);
    return;
  }

  proto::CacheMetadata metadata;
  metadata.set_entry_time(base::Time::Now().ToInternalValue());
  proto::CacheRequest* request_metadata = metadata.mutable_request();
  request_metadata->set_method(put_context->request->method);
  for (const auto& header : put_context->request->headers) {
    DCHECK_EQ(std::string::npos, header.first.find('\0'));
    DCHECK_EQ(std::string::npos, header.second.find('\0'));
    proto::CacheHeaderMap* header_map = request_metadata->add_headers();
    header_map->set_name(header.first);
    header_map->set_value(header.second);
  }

  proto::CacheResponse* response_metadata = metadata.mutable_response();
  response_metadata->set_status_code(put_context->response->status_code);
  response_metadata->set_status_text(put_context->response->status_text);
  response_metadata->set_response_type(FetchResponseTypeToProtoResponseType(
      put_context->response->response_type));
  for (const auto& url : put_context->response->url_list)
    response_metadata->add_url_list(url.spec());
  response_metadata->set_loaded_with_credentials(
      put_context->response->loaded_with_credentials);
  response_metadata->set_response_time(
      put_context->response->response_time.ToInternalValue());
  for (ResponseHeaderMap::const_iterator it =
           put_context->response->headers.begin();
       it != put_context->response->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    proto::CacheHeaderMap* header_map = response_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }
  for (const auto& header : put_context->response->cors_exposed_header_names)
    response_metadata->add_cors_exposed_header_names(header);

  std::unique_ptr<std::string> serialized(new std::string());
  if (!metadata.SerializeToString(serialized.get())) {
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kMetadataSerializationFailed));
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(std::move(serialized));

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = put_context->cache_entry.get();

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |put_context| is not
  // copyable.
  net::CompletionRepeatingCallback write_headers_callback =
      base::AdaptCallbackForRepeating(
          base::BindOnce(&LegacyCacheStorageCache::PutDidWriteHeaders,
                         weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                         buffer->size()));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  rv = temp_entry_ptr->WriteData(INDEX_HEADERS, 0 /* offset */, buffer.get(),
                                 buffer->size(), write_headers_callback,
                                 true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    std::move(write_headers_callback).Run(rv);
}

void LegacyCacheStorageCache::PutDidWriteHeaders(
    std::unique_ptr<PutContext> put_context,
    int expected_bytes,
    int rv) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::PutDidWriteHeaders",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (rv != expected_bytes) {
    quota_manager_proxy_->NotifyWriteFailed(origin_);
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kPutDidWriteHeadersWrongBytes));
    return;
  }

  if (ShouldPadResourceSize(*put_context->response)) {
    cache_padding_ += CalculateResponsePadding(*put_context->response,
                                               cache_padding_key_.get(),
                                               0 /* side_data_size */);
  }

  PutWriteBlobToCache(std::move(put_context), INDEX_RESPONSE_BODY);
}

void LegacyCacheStorageCache::PutWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    int disk_cache_body_index) {
  DCHECK(disk_cache_body_index == INDEX_RESPONSE_BODY ||
         disk_cache_body_index == INDEX_SIDE_DATA);

  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::PutWriteBlobToCache",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  mojo::PendingRemote<blink::mojom::Blob> blob;
  int64_t blob_size = 0;

  switch (disk_cache_body_index) {
    case INDEX_RESPONSE_BODY: {
      blob = std::move(put_context->blob);
      put_context->blob.reset();
      blob_size = put_context->blob_size;
      break;
    }
    case INDEX_SIDE_DATA: {
      blob = std::move(put_context->side_data_blob);
      put_context->side_data_blob.reset();
      blob_size = put_context->side_data_blob_size;
      break;
    }
    case INDEX_HEADERS:
      NOTREACHED();
  }

  ScopedWritableEntry entry(put_context->cache_entry.release());

  // If there isn't blob data for this index, then we may need to clear any
  // pre-existing data.  This can happen under rare circumstances if a stale
  // file is present and accepted by OpenOrCreateEntry().
  if (!blob) {
    disk_cache::Entry* temp_entry_ptr = entry.get();

    net::CompletionRepeatingCallback clear_callback =
        base::AdaptCallbackForRepeating(base::BindOnce(
            &LegacyCacheStorageCache::PutWriteBlobToCacheComplete,
            weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
            disk_cache_body_index, std::move(entry)));

    // If there is no pre-existing data, then proceed to the next
    // step immediately.
    if (temp_entry_ptr->GetDataSize(disk_cache_body_index) != 0) {
      std::move(clear_callback).Run(net::OK);
      return;
    }

    // There is pre-existing data and we need to truncate it.
    int rv = temp_entry_ptr->WriteData(
        disk_cache_body_index, /* offset = */ 0, /* buf = */ nullptr,
        /* buf_len = */ 0, clear_callback, /* truncate = */ true);

    if (rv != net::ERR_IO_PENDING)
      std::move(clear_callback).Run(rv);

    return;
  }

  // We have real data, so stream it into the entry.  This will overwrite
  // any existing data.
  auto blob_to_cache = std::make_unique<CacheStorageBlobToDiskCache>(
      quota_manager_proxy_, origin_);
  CacheStorageBlobToDiskCache* blob_to_cache_raw = blob_to_cache.get();
  BlobToDiskCacheIDMap::KeyType blob_to_cache_key =
      active_blob_to_disk_cache_writers_.Add(std::move(blob_to_cache));

  blob_to_cache_raw->StreamBlobToCache(
      std::move(entry), disk_cache_body_index, std::move(blob), blob_size,
      base::BindOnce(&LegacyCacheStorageCache::PutDidWriteBlobToCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(put_context),
                     blob_to_cache_key, disk_cache_body_index));
}

void LegacyCacheStorageCache::PutDidWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
    int disk_cache_body_index,
    ScopedWritableEntry entry,
    bool success) {
  DCHECK(entry);
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::PutDidWriteBlobToCache",
                         TRACE_ID_GLOBAL(put_context->trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  active_blob_to_disk_cache_writers_.Remove(blob_to_cache_key);

  PutWriteBlobToCacheComplete(std::move(put_context), disk_cache_body_index,
                              std::move(entry),
                              (success ? net::OK : net::ERR_FAILED));
}

void LegacyCacheStorageCache::PutWriteBlobToCacheComplete(
    std::unique_ptr<PutContext> put_context,
    int disk_cache_body_index,
    ScopedWritableEntry entry,
    int rv) {
  DCHECK(entry);

  put_context->cache_entry = std::move(entry);

  if (rv != net::OK) {
    PutComplete(
        std::move(put_context),
        MakeErrorStorage(ErrorStorageType::kPutDidWriteBlobToCacheFailed));
    return;
  }

  if (disk_cache_body_index == INDEX_RESPONSE_BODY) {
    PutWriteBlobToCache(std::move(put_context), INDEX_SIDE_DATA);
    return;
  }

  PutComplete(std::move(put_context), CacheStorageError::kSuccess);
}

void LegacyCacheStorageCache::PutComplete(
    std::unique_ptr<PutContext> put_context,
    blink::mojom::CacheStorageError error) {
  if (error == CacheStorageError::kSuccess) {
    // Make sure we've written everything.
    DCHECK(put_context->cache_entry);
    DCHECK(!put_context->blob);
    DCHECK(!put_context->side_data_blob);

    // Tell the WritableScopedEntry not to doom the entry since it was a
    // successful operation.
    put_context->cache_entry.get_deleter().WritingCompleted();
  }

  UpdateCacheSize(base::BindOnce(std::move(put_context->callback), error));
}

void LegacyCacheStorageCache::CalculateCacheSizePadding(
    SizePaddingCallback got_sizes_callback) {
  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |got_sizes_callback| is not
  // copyable.
  net::Int64CompletionRepeatingCallback got_size_callback =
      base::AdaptCallbackForRepeating(base::BindOnce(
          &LegacyCacheStorageCache::CalculateCacheSizePaddingGotSize,
          weak_ptr_factory_.GetWeakPtr(), std::move(got_sizes_callback)));

  int64_t rv = backend_->CalculateSizeOfAllEntries(got_size_callback);
  if (rv != net::ERR_IO_PENDING)
    std::move(got_size_callback).Run(rv);
}

void LegacyCacheStorageCache::CalculateCacheSizePaddingGotSize(
    SizePaddingCallback callback,
    int64_t cache_size) {
  // Enumerating entries is only done during cache initialization and only if
  // necessary.
  DCHECK_EQ(backend_state_, BACKEND_UNINITIALIZED);
  auto request = blink::mojom::FetchAPIRequest::New();
  blink::mojom::CacheQueryOptionsPtr options =
      blink::mojom::CacheQueryOptions::New();
  options->ignore_search = true;
  QueryCache(std::move(request), std::move(options),
             QUERY_CACHE_RESPONSES_NO_BODIES,
             CacheStorageSchedulerPriority::kNormal,
             base::BindOnce(&LegacyCacheStorageCache::PaddingDidQueryCache,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            cache_size));
}

void LegacyCacheStorageCache::PaddingDidQueryCache(
    SizePaddingCallback callback,
    int64_t cache_size,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  int64_t cache_padding = 0;
  if (error == CacheStorageError::kSuccess) {
    for (const auto& result : *query_cache_results) {
      if (ShouldPadResourceSize(*result.response)) {
        int32_t side_data_size =
            result.entry ? result.entry->GetDataSize(INDEX_SIDE_DATA) : 0;
        cache_padding += CalculateResponsePadding(
            *result.response, cache_padding_key_.get(), side_data_size);
      }
    }
  }

  std::move(callback).Run(cache_size, cache_padding);
}

void LegacyCacheStorageCache::CalculateCacheSize(
    net::Int64CompletionOnceCallback callback) {
  net::Int64CompletionRepeatingCallback got_size_callback =
      AdaptCallbackForRepeating(std::move(callback));
  int64_t rv = backend_->CalculateSizeOfAllEntries(got_size_callback);
  if (rv != net::ERR_IO_PENDING)
    got_size_callback.Run(rv);
}

void LegacyCacheStorageCache::UpdateCacheSize(base::OnceClosure callback) {
  if (backend_state_ != BACKEND_OPEN)
    return;

  // Note that the callback holds a cache handle to keep the cache alive during
  // the operation since this UpdateCacheSize is often run after an operation
  // completes and runs its callback.
  CalculateCacheSize(base::AdaptCallbackForRepeating(base::BindOnce(
      &LegacyCacheStorageCache::UpdateCacheSizeGotSize,
      weak_ptr_factory_.GetWeakPtr(), CreateHandle(), std::move(callback))));
}

void LegacyCacheStorageCache::UpdateCacheSizeGotSize(
    CacheStorageCacheHandle cache_handle,
    base::OnceClosure callback,
    int64_t current_cache_size) {
  DCHECK_NE(current_cache_size, CacheStorage::kSizeUnknown);
  cache_size_ = current_cache_size;
  int64_t size_delta = PaddedCacheSize() - last_reported_size_;
  last_reported_size_ = PaddedCacheSize();

  quota_manager_proxy_->NotifyStorageModified(
      CacheStorageQuotaClient::GetClientTypeFromOwner(owner_), origin_,
      blink::mojom::StorageType::kTemporary, size_delta);

  if (cache_storage_)
    cache_storage_->NotifyCacheContentChanged(cache_name_);

  if (cache_observer_)
    cache_observer_->CacheSizeUpdated(this);

  std::move(callback).Run();
}

void LegacyCacheStorageCache::GetAllMatchedEntries(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    CacheEntriesCallback callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysBackendClosed), {});
    return;
  }

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kShared,
      CacheStorageSchedulerOp::kGetAllMatched,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::GetAllMatchedEntriesImpl,
          weak_ptr_factory_.GetWeakPtr(), std::move(request),
          std::move(options), trace_id,
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::GetAllMatchedEntriesImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    CacheEntriesCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2("CacheStorage",
                         "LegacyCacheStorageCache::GetAllMatchedEntriesImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(request), "options",
                         CacheStorageTracedValue(options));
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(
            ErrorStorageType::kStorageGetAllMatchedEntriesBackendClosed),
        {});
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(
      std::move(request), std::move(options),
      QUERY_CACHE_REQUESTS | QUERY_CACHE_RESPONSES_WITH_BODIES,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::GetAllMatchedEntriesDidQueryCache,
          weak_ptr_factory_.GetWeakPtr(), trace_id, std::move(callback)));
}

void LegacyCacheStorageCache::GetAllMatchedEntriesDidQueryCache(
    int64_t trace_id,
    CacheEntriesCallback callback,
    blink::mojom::CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  TRACE_EVENT_WITH_FLOW0(
      "CacheStorage",
      "LegacyCacheStorageCache::GetAllMatchedEntriesDidQueryCache",
      TRACE_ID_GLOBAL(trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error, {});
    return;
  }

  std::vector<CacheEntry> entries;
  entries.reserve(query_cache_results->size());

  for (auto& result : *query_cache_results) {
    entries.emplace_back(std::move(result.request), std::move(result.response));
  }

  std::move(callback).Run(CacheStorageError::kSuccess, std::move(entries));
}

CacheStorageCache::InitState LegacyCacheStorageCache::GetInitState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initializing_ ? InitState::Initializing : InitState::Initialized;
}

void LegacyCacheStorageCache::Delete(blink::mojom::BatchOperationPtr operation,
                                     ErrorCallback callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(blink::mojom::OperationType::kDelete, operation->operation_type);

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = operation->request->url;
  request->method = operation->request->method;
  request->is_reload = operation->request->is_reload;
  request->referrer = operation->request->referrer.Clone();
  request->headers = operation->request->headers;

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive,
      CacheStorageSchedulerOp::kDelete, CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::DeleteImpl, weak_ptr_factory_.GetWeakPtr(),
          std::move(request), std::move(operation->match_options),
          scheduler_->WrapCallbackToRunNext(id, std::move(callback))));
}

void LegacyCacheStorageCache::DeleteImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    ErrorCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kDeleteImplBackendClosed));
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(
      std::move(request), std::move(match_options),
      QUERY_CACHE_ENTRIES | QUERY_CACHE_RESPONSES_NO_BODIES,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(&LegacyCacheStorageCache::DeleteDidQueryCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LegacyCacheStorageCache::DeleteDidQueryCache(
    ErrorCallback callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error);
    return;
  }

  if (query_cache_results->empty()) {
    std::move(callback).Run(CacheStorageError::kErrorNotFound);
    return;
  }

  DCHECK(scheduler_->IsRunningExclusiveOperation());

  for (auto& result : *query_cache_results) {
    disk_cache::ScopedEntryPtr entry = std::move(result.entry);
    if (ShouldPadResourceSize(*result.response)) {
      cache_padding_ -=
          CalculateResponsePadding(*result.response, cache_padding_key_.get(),
                                   entry->GetDataSize(INDEX_SIDE_DATA));
    }
    entry->Doom();
  }

  UpdateCacheSize(
      base::BindOnce(std::move(callback), CacheStorageError::kSuccess));
}

void LegacyCacheStorageCache::KeysImpl(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    RequestsCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "LegacyCacheStorageCache::KeysImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(request), "options",
                         CacheStorageTracedValue(options));

  if (backend_state_ != BACKEND_OPEN) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kKeysImplBackendClosed), nullptr);
    return;
  }

  // Hold the cache alive while performing any operation touching the
  // disk_cache backend.
  callback = WrapCallbackWithHandle(std::move(callback));

  QueryCache(std::move(request), std::move(options), QUERY_CACHE_REQUESTS,
             CacheStorageSchedulerPriority::kNormal,
             base::BindOnce(&LegacyCacheStorageCache::KeysDidQueryCache,
                            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                            trace_id));
}

void LegacyCacheStorageCache::KeysDidQueryCache(
    RequestsCallback callback,
    int64_t trace_id,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage",
                         "LegacyCacheStorageCache::KeysDidQueryCache",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (error != CacheStorageError::kSuccess) {
    std::move(callback).Run(error, nullptr);
    return;
  }

  std::unique_ptr<Requests> out_requests = std::make_unique<Requests>();
  out_requests->reserve(query_cache_results->size());
  for (auto& result : *query_cache_results)
    out_requests->push_back(std::move(result.request));
  std::move(callback).Run(CacheStorageError::kSuccess, std::move(out_requests));
}

void LegacyCacheStorageCache::CloseImpl(base::OnceClosure callback) {
  DCHECK_EQ(BACKEND_OPEN, backend_state_);

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  backend_.reset();
  post_backend_closed_callback_ = std::move(callback);
}

void LegacyCacheStorageCache::DeleteBackendCompletedIO() {
  if (!post_backend_closed_callback_.is_null()) {
    DCHECK_NE(BACKEND_CLOSED, backend_state_);
    backend_state_ = BACKEND_CLOSED;
    std::move(post_backend_closed_callback_).Run();
  }
}

void LegacyCacheStorageCache::SizeImpl(SizeCallback callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);

  // TODO(cmumford): Can CacheStorage::kSizeUnknown be returned instead of zero?
  if (backend_state_ != BACKEND_OPEN) {
    scheduler_task_runner_->PostTask(FROM_HERE,
                                     base::BindOnce(std::move(callback), 0));
    return;
  }

  int64_t size = backend_state_ == BACKEND_OPEN ? PaddedCacheSize() : 0;
  scheduler_task_runner_->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(callback), size));
}

void LegacyCacheStorageCache::GetSizeThenCloseDidGetSize(SizeCallback callback,
                                                         int64_t cache_size) {
  cache_entry_handler_->InvalidateDiskCacheBlobEntrys();
  CloseImpl(base::BindOnce(std::move(callback), cache_size));
}

void LegacyCacheStorageCache::CreateBackend(ErrorCallback callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type = memory_only_ ? net::MEMORY_CACHE : net::APP_CACHE;

  // The maximum size of each cache. Ultimately, cache size
  // is controlled per-origin by the QuotaManager.
  int64_t max_bytes = memory_only_ ? std::numeric_limits<int>::max()
                                   : std::numeric_limits<int64_t>::max();

  std::unique_ptr<ScopedBackendPtr> backend_ptr(new ScopedBackendPtr());

  // Temporary pointer so that backend_ptr can be Pass()'d in Bind below.
  ScopedBackendPtr* backend = backend_ptr.get();

  // Create a callback that is copyable, even though it can only be called once.
  // BindRepeating() cannot be used directly because |callback| and
  // |backend_ptr| are not copyable.
  net::CompletionRepeatingCallback create_cache_callback =
      base::AdaptCallbackForRepeating(
          base::BindOnce(&LegacyCacheStorageCache::CreateBackendDidCreate,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(backend_ptr)));

  DCHECK(scheduler_->IsRunningExclusiveOperation());
  int rv = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, path_, max_bytes,
      disk_cache::ResetHandling::kNeverReset, nullptr, backend,
      base::BindOnce(&LegacyCacheStorageCache::DeleteBackendCompletedIO,
                     weak_ptr_factory_.GetWeakPtr()),
      create_cache_callback);
  if (rv != net::ERR_IO_PENDING)
    std::move(create_cache_callback).Run(rv);
}

void LegacyCacheStorageCache::CreateBackendDidCreate(
    LegacyCacheStorageCache::ErrorCallback callback,
    std::unique_ptr<ScopedBackendPtr> backend_ptr,
    int rv) {
  if (rv != net::OK) {
    std::move(callback).Run(
        MakeErrorStorage(ErrorStorageType::kCreateBackendDidCreateFailed));
    return;
  }

  backend_ = std::move(*backend_ptr);
  std::move(callback).Run(CacheStorageError::kSuccess);
}

void LegacyCacheStorageCache::InitBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(BACKEND_UNINITIALIZED, backend_state_);
  DCHECK(!initializing_);
  DCHECK(!scheduler_->ScheduledOperations());
  initializing_ = true;

  auto id = scheduler_->CreateId();
  scheduler_->ScheduleOperation(
      id, CacheStorageSchedulerMode::kExclusive, CacheStorageSchedulerOp::kInit,
      CacheStorageSchedulerPriority::kNormal,
      base::BindOnce(
          &LegacyCacheStorageCache::CreateBackend,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &LegacyCacheStorageCache::InitDidCreateBackend,
              weak_ptr_factory_.GetWeakPtr(),
              scheduler_->WrapCallbackToRunNext(id, base::DoNothing::Once()))));
}

void LegacyCacheStorageCache::InitDidCreateBackend(
    base::OnceClosure callback,
    CacheStorageError cache_create_error) {
  if (cache_create_error != CacheStorageError::kSuccess) {
    InitGotCacheSize(std::move(callback), cache_create_error, 0);
    return;
  }

  auto calculate_size_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  int64_t rv = backend_->CalculateSizeOfAllEntries(
      base::BindOnce(&LegacyCacheStorageCache::InitGotCacheSize,
                     weak_ptr_factory_.GetWeakPtr(), calculate_size_callback,
                     cache_create_error));

  if (rv != net::ERR_IO_PENDING)
    InitGotCacheSize(std::move(calculate_size_callback), cache_create_error,
                     rv);
}

void LegacyCacheStorageCache::InitGotCacheSize(
    base::OnceClosure callback,
    CacheStorageError cache_create_error,
    int64_t cache_size) {
  if (cache_create_error != CacheStorageError::kSuccess) {
    InitGotCacheSizeAndPadding(std::move(callback), cache_create_error, 0, 0);
    return;
  }

  // Now that we know the cache size either 1) the cache size should be unknown
  // (which is why the size was calculated), or 2) it must match the current
  // size. If the sizes aren't equal then there is a bug in how the cache size
  // is saved in the store's index.
  if (cache_size_ != CacheStorage::kSizeUnknown) {
    DLOG_IF(ERROR, cache_size_ != cache_size)
        << "Cache size: " << cache_size
        << " does not match size from index: " << cache_size_;
    if (cache_size_ != cache_size) {
      // We assume that if the sizes match then then cached padding is still
      // correct. If not then we recalculate the padding.
      CalculateCacheSizePaddingGotSize(
          base::BindOnce(&LegacyCacheStorageCache::InitGotCacheSizeAndPadding,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         cache_create_error),
          cache_size);
      return;
    }
  }

  if (cache_padding_ == CacheStorage::kSizeUnknown || cache_padding_ < 0) {
    CalculateCacheSizePaddingGotSize(
        base::BindOnce(&LegacyCacheStorageCache::InitGotCacheSizeAndPadding,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       cache_create_error),
        cache_size);
    return;
  }

  // If cached size matches actual size then assume cached padding is still
  // correct.
  InitGotCacheSizeAndPadding(std::move(callback), cache_create_error,
                             cache_size, cache_padding_);
}

void LegacyCacheStorageCache::InitGotCacheSizeAndPadding(
    base::OnceClosure callback,
    CacheStorageError cache_create_error,
    int64_t cache_size,
    int64_t cache_padding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_size_ = cache_size;
  cache_padding_ = cache_padding;

  initializing_ = false;
  backend_state_ = (cache_create_error == CacheStorageError::kSuccess &&
                    backend_ && backend_state_ == BACKEND_UNINITIALIZED)
                       ? BACKEND_OPEN
                       : BACKEND_CLOSED;

  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.InitBackendResult",
                            cache_create_error);

  if (cache_observer_)
    cache_observer_->CacheSizeUpdated(this);

  std::move(callback).Run();
}

int64_t LegacyCacheStorageCache::PaddedCacheSize() const {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (cache_size_ == CacheStorage::kSizeUnknown ||
      cache_padding_ == CacheStorage::kSizeUnknown) {
    return CacheStorage::kSizeUnknown;
  }
  return cache_size_ + cache_padding_;
}

base::CheckedNumeric<uint64_t>
LegacyCacheStorageCache::CalculateRequiredSafeSpaceForPut(
    const blink::mojom::BatchOperationPtr& operation) {
  DCHECK_EQ(blink::mojom::OperationType::kPut, operation->operation_type);
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  safe_space_required +=
      CalculateRequiredSafeSpaceForResponse(operation->response);
  safe_space_required +=
      CalculateRequiredSafeSpaceForRequest(operation->request);

  return safe_space_required;
}

base::CheckedNumeric<uint64_t>
LegacyCacheStorageCache::CalculateRequiredSafeSpaceForRequest(
    const blink::mojom::FetchAPIRequestPtr& request) {
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  safe_space_required += request->method.size();

  safe_space_required += request->url.spec().size();

  for (const auto& header : request->headers) {
    safe_space_required += header.first.size();
    safe_space_required += header.second.size();
  }

  return safe_space_required;
}

base::CheckedNumeric<uint64_t>
LegacyCacheStorageCache::CalculateRequiredSafeSpaceForResponse(
    const blink::mojom::FetchAPIResponsePtr& response) {
  base::CheckedNumeric<uint64_t> safe_space_required = 0;
  safe_space_required += (response->blob ? response->blob->size : 0);
  safe_space_required += response->status_text.size();

  for (const auto& header : response->headers) {
    safe_space_required += header.first.size();
    safe_space_required += header.second.size();
  }

  for (const auto& header : response->cors_exposed_header_names) {
    safe_space_required += header.size();
  }

  for (const auto& url : response->url_list) {
    safe_space_required += url.spec().size();
  }

  return safe_space_required;
}

}  // namespace content
