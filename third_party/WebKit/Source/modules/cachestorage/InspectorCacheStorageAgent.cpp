// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/InspectorCacheStorageAgent.h"

#include <algorithm>
#include <memory>
#include <utility>
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

using blink::protocol::Array;
// Renaming Cache since there is another blink::Cache.
using ProtocolCache = blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::CachedResponse;
using blink::protocol::CacheStorage::DataEntry;
using blink::protocol::CacheStorage::Header;
// Renaming Response since there is another blink::Response.
using ProtocolResponse = blink::protocol::Response;

using DeleteCacheCallback =
    blink::protocol::CacheStorage::Backend::DeleteCacheCallback;
using DeleteEntryCallback =
    blink::protocol::CacheStorage::Backend::DeleteEntryCallback;
using RequestCacheNamesCallback =
    blink::protocol::CacheStorage::Backend::RequestCacheNamesCallback;
using RequestEntriesCallback =
    blink::protocol::CacheStorage::Backend::RequestEntriesCallback;
using RequestCachedResponseCallback =
    blink::protocol::CacheStorage::Backend::RequestCachedResponseCallback;
using BatchOperation = blink::WebServiceWorkerCache::BatchOperation;

namespace blink {

namespace {

String BuildCacheId(const String& security_origin, const String& cache_name) {
  String id(security_origin);
  id.append('|');
  id.append(cache_name);
  return id;
}

ProtocolResponse ParseCacheId(const String& id,
                              String* security_origin,
                              String* cache_name) {
  size_t pipe = id.find('|');
  if (pipe == WTF::kNotFound)
    return ProtocolResponse::Error("Invalid cache id.");
  *security_origin = id.Substring(0, pipe);
  *cache_name = id.Substring(pipe + 1);
  return ProtocolResponse::OK();
}

ProtocolResponse AssertCacheStorage(
    const String& security_origin,
    std::unique_ptr<WebServiceWorkerCacheStorage>* result) {
  RefPtr<SecurityOrigin> sec_origin =
      SecurityOrigin::CreateFromString(security_origin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!sec_origin->IsPotentiallyTrustworthy()) {
    return ProtocolResponse::Error(
        sec_origin->IsPotentiallyTrustworthyErrorMessage());
  }

  std::unique_ptr<WebServiceWorkerCacheStorage> cache =
      Platform::Current()->CreateCacheStorage(WebSecurityOrigin(sec_origin));
  if (!cache)
    return ProtocolResponse::Error("Could not find cache storage.");
  *result = std::move(cache);
  return ProtocolResponse::OK();
}

ProtocolResponse AssertCacheStorageAndNameForId(
    const String& cache_id,
    String* cache_name,
    std::unique_ptr<WebServiceWorkerCacheStorage>* result) {
  String security_origin;
  ProtocolResponse response =
      ParseCacheId(cache_id, &security_origin, cache_name);
  if (!response.isSuccess())
    return response;
  return AssertCacheStorage(security_origin, result);
}

CString ServiceWorkerCacheErrorString(WebServiceWorkerCacheError error) {
  switch (error) {
    case kWebServiceWorkerCacheErrorNotImplemented:
      return CString("not implemented.");
      break;
    case kWebServiceWorkerCacheErrorNotFound:
      return CString("not found.");
      break;
    case kWebServiceWorkerCacheErrorExists:
      return CString("cache already exists.");
      break;
    case kWebServiceWorkerCacheErrorQuotaExceeded:
      return CString("quota exceeded.");
    case kWebServiceWorkerCacheErrorCacheNameNotFound:
      return CString("cache not found.");
    case kWebServiceWorkerCacheErrorTooLarge:
      return CString("operation too large.");
  }
  NOTREACHED();
  return "";
}

ProtocolResponse GetExecutionContext(InspectedFrames* frames,
                                     const String& cache_id,
                                     ExecutionContext** context) {
  String origin;
  String id;
  ProtocolResponse res = ParseCacheId(cache_id, &origin, &id);
  if (!res.isSuccess())
    return res;

  LocalFrame* frame = frames->FrameWithSecurityOrigin(origin);
  if (!frame)
    return ProtocolResponse::Error("No frame with origin " + origin);

  blink::Document* document = frame->GetDocument();
  if (!document)
    return ProtocolResponse::Error("No execution context found");

  *context = document;

  return ProtocolResponse::OK();
}

class RequestCacheNames
    : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
  WTF_MAKE_NONCOPYABLE(RequestCacheNames);

 public:
  RequestCacheNames(const String& security_origin,
                    std::unique_ptr<RequestCacheNamesCallback> callback)
      : security_origin_(security_origin), callback_(std::move(callback)) {}

  ~RequestCacheNames() override {}

  void OnSuccess(const WebVector<WebString>& caches) override {
    std::unique_ptr<Array<ProtocolCache>> array =
        Array<ProtocolCache>::create();
    for (size_t i = 0; i < caches.size(); i++) {
      String name = String(caches[i]);
      std::unique_ptr<ProtocolCache> entry =
          ProtocolCache::create()
              .setSecurityOrigin(security_origin_)
              .setCacheName(name)
              .setCacheId(BuildCacheId(security_origin_, name))
              .build();
      array->addItem(std::move(entry));
    }
    callback_->sendSuccess(std::move(array));
  }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(
        String::Format("Error requesting cache names: %s",
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  String security_origin_;
  std::unique_ptr<RequestCacheNamesCallback> callback_;
};

struct DataRequestParams {
  String cache_name;
  int skip_count;
  int page_size;
};

struct RequestResponse {
  String request_url;
  String request_method;
  HTTPHeaderMap request_headers;
  int response_status;
  String response_status_text;
  double response_time;
  HTTPHeaderMap response_headers;
};

class ResponsesAccumulator : public RefCounted<ResponsesAccumulator> {
  WTF_MAKE_NONCOPYABLE(ResponsesAccumulator);

 public:
  ResponsesAccumulator(int num_responses,
                       const DataRequestParams& params,
                       std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params),
        num_responses_left_(num_responses),
        responses_(static_cast<size_t>(num_responses)),
        callback_(std::move(callback)) {}

  void AddRequestResponsePair(const WebServiceWorkerRequest& request,
                              const WebServiceWorkerResponse& response) {
    DCHECK_GT(num_responses_left_, 0);
    RequestResponse& request_response =
        responses_.at(responses_.size() - num_responses_left_);

    request_response.request_url = request.Url().GetString();
    request_response.request_method = request.Method();
    request_response.request_headers = request.Headers();
    request_response.response_status = response.Status();
    request_response.response_status_text = response.StatusText();
    request_response.response_time = response.ResponseTime().ToDoubleT();
    request_response.response_headers = response.Headers();

    if (--num_responses_left_ != 0)
      return;

    std::sort(responses_.begin(), responses_.end(),
              [](const RequestResponse& a, const RequestResponse& b) {
                return WTF::CodePointCompareLessThan(a.request_url,
                                                     b.request_url);
              });
    if (params_.skip_count > 0)
      responses_.EraseAt(0, params_.skip_count);
    bool has_more = false;
    if (static_cast<size_t>(params_.page_size) < responses_.size()) {
      responses_.EraseAt(params_.page_size,
                         responses_.size() - params_.page_size);
      has_more = true;
    }
    std::unique_ptr<Array<DataEntry>> array = Array<DataEntry>::create();
    for (const auto& request_response : responses_) {
      std::unique_ptr<DataEntry> entry =
          DataEntry::create()
              .setRequestURL(request_response.request_url)
              .setRequestMethod(request_response.request_method)
              .setRequestHeaders(
                  SerializeHeaders(request_response.request_headers))
              .setResponseStatus(request_response.response_status)
              .setResponseStatusText(request_response.response_status_text)
              .setResponseTime(request_response.response_time)
              .setResponseHeaders(
                  SerializeHeaders(request_response.response_headers))
              .build();
      array->addItem(std::move(entry));
    }
    callback_->sendSuccess(std::move(array), has_more);
  }

  void SendFailure(const ProtocolResponse& error) {
    callback_->sendFailure(error);
  }

  std::unique_ptr<Array<Header>> SerializeHeaders(
      const HTTPHeaderMap& headers) {
    std::unique_ptr<Array<Header>> result = Array<Header>::create();
    for (HTTPHeaderMap::const_iterator it = headers.begin(),
                                       end = headers.end();
         it != end; ++it) {
      result->addItem(
          Header::create().setName(it->key).setValue(it->value).build());
    }
    return result;
  }

 private:
  DataRequestParams params_;
  int num_responses_left_;
  Vector<RequestResponse> responses_;
  std::unique_ptr<RequestEntriesCallback> callback_;
};

class GetCacheResponsesForRequestData
    : public WebServiceWorkerCache::CacheMatchCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheResponsesForRequestData);

 public:
  GetCacheResponsesForRequestData(const DataRequestParams& params,
                                  const WebServiceWorkerRequest& request,
                                  RefPtr<ResponsesAccumulator> accum)
      : params_(params), request_(request), accumulator_(std::move(accum)) {}
  ~GetCacheResponsesForRequestData() override {}

  void OnSuccess(const WebServiceWorkerResponse& response) override {
    accumulator_->AddRequestResponsePair(request_, response);
  }

  void OnError(WebServiceWorkerCacheError error) override {
    accumulator_->SendFailure(ProtocolResponse::Error(
        String::Format("Error requesting responses for cache  %s: %s",
                       params_.cache_name.Utf8().data(),
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  DataRequestParams params_;
  WebServiceWorkerRequest request_;
  RefPtr<ResponsesAccumulator> accumulator_;
};

class GetCacheKeysForRequestData
    : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheKeysForRequestData);

 public:
  GetCacheKeysForRequestData(const DataRequestParams& params,
                             std::unique_ptr<WebServiceWorkerCache> cache,
                             std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params),
        cache_(std::move(cache)),
        callback_(std::move(callback)) {}
  ~GetCacheKeysForRequestData() override {}

  WebServiceWorkerCache* Cache() { return cache_.get(); }
  void OnSuccess(const WebVector<WebServiceWorkerRequest>& requests) override {
    if (requests.IsEmpty()) {
      std::unique_ptr<Array<DataEntry>> array = Array<DataEntry>::create();
      callback_->sendSuccess(std::move(array), false);
      return;
    }
    RefPtr<ResponsesAccumulator> accumulator =
        WTF::AdoptRef(new ResponsesAccumulator(requests.size(), params_,
                                               std::move(callback_)));

    for (size_t i = 0; i < requests.size(); i++) {
      const auto& request = requests[i];
      auto cache_request = WTF::MakeUnique<GetCacheResponsesForRequestData>(
          params_, request, accumulator);
      cache_->DispatchMatch(std::move(cache_request), request,
                            WebServiceWorkerCache::QueryParams());
    }
  }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(
        String::Format("Error requesting requests for cache %s: %s",
                       params_.cache_name.Utf8().data(),
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  DataRequestParams params_;
  std::unique_ptr<WebServiceWorkerCache> cache_;
  std::unique_ptr<RequestEntriesCallback> callback_;
};

class GetCacheForRequestData
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheForRequestData);

 public:
  GetCacheForRequestData(const DataRequestParams& params,
                         std::unique_ptr<RequestEntriesCallback> callback)
      : params_(params), callback_(std::move(callback)) {}
  ~GetCacheForRequestData() override {}

  void OnSuccess(std::unique_ptr<WebServiceWorkerCache> cache) override {
    auto cache_request = WTF::MakeUnique<GetCacheKeysForRequestData>(
        params_, std::move(cache), std::move(callback_));
    cache_request->Cache()->DispatchKeys(std::move(cache_request),
                                         WebServiceWorkerRequest(),
                                         WebServiceWorkerCache::QueryParams());
  }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(String::Format(
        "Error requesting cache %s: %s", params_.cache_name.Utf8().data(),
        ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  DataRequestParams params_;
  std::unique_ptr<RequestEntriesCallback> callback_;
};

class DeleteCache : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCache);

 public:
  explicit DeleteCache(std::unique_ptr<DeleteCacheCallback> callback)
      : callback_(std::move(callback)) {}
  ~DeleteCache() override {}

  void OnSuccess() override { callback_->sendSuccess(); }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(
        String::Format("Error requesting cache names: %s",
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  std::unique_ptr<DeleteCacheCallback> callback_;
};

class DeleteCacheEntry : public WebServiceWorkerCache::CacheBatchCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCacheEntry);

 public:
  explicit DeleteCacheEntry(std::unique_ptr<DeleteEntryCallback> callback)
      : callback_(std::move(callback)) {}
  ~DeleteCacheEntry() override {}

  void OnSuccess() override { callback_->sendSuccess(); }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(
        String::Format("Error requesting cache names: %s",
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  std::unique_ptr<DeleteEntryCallback> callback_;
};

class GetCacheForDeleteEntry
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheForDeleteEntry);

 public:
  GetCacheForDeleteEntry(const String& request_spec,
                         const String& cache_name,
                         std::unique_ptr<DeleteEntryCallback> callback)
      : request_spec_(request_spec),
        cache_name_(cache_name),
        callback_(std::move(callback)) {}
  ~GetCacheForDeleteEntry() override {}

  void OnSuccess(std::unique_ptr<WebServiceWorkerCache> cache) override {
    auto delete_request =
        WTF::MakeUnique<DeleteCacheEntry>(std::move(callback_));
    BatchOperation delete_operation;
    delete_operation.operation_type =
        WebServiceWorkerCache::kOperationTypeDelete;
    delete_operation.request.SetURL(KURL(kParsedURLString, request_spec_));
    Vector<BatchOperation> operations;
    operations.push_back(delete_operation);
    cache.release()->DispatchBatch(std::move(delete_request),
                                   WebVector<BatchOperation>(operations));
  }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(String::Format(
        "Error requesting cache %s: %s", cache_name_.Utf8().data(),
        ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  String request_spec_;
  String cache_name_;
  std::unique_ptr<DeleteEntryCallback> callback_;
};

class CachedResponseFileReaderLoaderClient final
    : private FileReaderLoaderClient {
  WTF_MAKE_NONCOPYABLE(CachedResponseFileReaderLoaderClient);

 public:
  static void Load(ExecutionContext* context,
                   RefPtr<BlobDataHandle> blob,
                   std::unique_ptr<RequestCachedResponseCallback> callback) {
    new CachedResponseFileReaderLoaderClient(context, std::move(blob),
                                             std::move(callback));
  }

  void DidStartLoading() override {}

  void DidFinishLoading() override {
    std::unique_ptr<CachedResponse> response =
        CachedResponse::create()
            .setBody(Base64Encode(data_->Data(), data_->size()))
            .build();
    callback_->sendSuccess(std::move(response));
    dispose();
  }

  void DidFail(FileError::ErrorCode error) {
    callback_->sendFailure(ProtocolResponse::Error(String::Format(
        "Unable to read the cached response, error code: %d", error)));
    dispose();
  }

  void DidReceiveDataForClient(const char* data,
                               unsigned data_length) override {
    data_->Append(data, data_length);
  }

 private:
  CachedResponseFileReaderLoaderClient(
      ExecutionContext* context,
      RefPtr<BlobDataHandle>&& blob,
      std::unique_ptr<RequestCachedResponseCallback>&& callback)
      : loader_(
            FileReaderLoader::Create(FileReaderLoader::kReadByClient, this)),
        callback_(std::move(callback)),
        data_(SharedBuffer::Create()) {
    loader_->Start(context, std::move(blob));
  }

  ~CachedResponseFileReaderLoaderClient() {}

  void dispose() { delete this; }

  std::unique_ptr<FileReaderLoader> loader_;
  std::unique_ptr<RequestCachedResponseCallback> callback_;
  RefPtr<SharedBuffer> data_;
};

class CachedResponseMatchCallback
    : public WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks {
  WTF_MAKE_NONCOPYABLE(CachedResponseMatchCallback);

 public:
  CachedResponseMatchCallback(
      ExecutionContext* context,
      std::unique_ptr<RequestCachedResponseCallback> callback)
      : callback_(std::move(callback)), context_(context) {}

  void OnSuccess(const WebServiceWorkerResponse& response) override {
    std::unique_ptr<protocol::DictionaryValue> headers =
        protocol::DictionaryValue::create();
    if (!response.GetBlobDataHandle()) {
      callback_->sendSuccess(CachedResponse::create()
                                 .setBody("")
                                 .build());
      return;
    }
    CachedResponseFileReaderLoaderClient::Load(
        context_, response.GetBlobDataHandle(), std::move(callback_));
  }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(ProtocolResponse::Error(
        String::Format("Unable to read cached response: %s",
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  std::unique_ptr<RequestCachedResponseCallback> callback_;
  Persistent<ExecutionContext> context_;
};
}  // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent(InspectedFrames* frames)
    : frames_(frames) {}

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() = default;

void InspectorCacheStorageAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorCacheStorageAgent::requestCacheNames(
    const String& security_origin,
    std::unique_ptr<RequestCacheNamesCallback> callback) {
  RefPtr<SecurityOrigin> sec_origin =
      SecurityOrigin::CreateFromString(security_origin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!sec_origin->IsPotentiallyTrustworthy()) {
    // Don't treat this as an error, just don't attempt to open and enumerate
    // the caches.
    callback->sendSuccess(Array<ProtocolCache>::create());
    return;
  }

  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  ProtocolResponse response = AssertCacheStorage(security_origin, &cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->DispatchKeys(
      WTF::MakeUnique<RequestCacheNames>(security_origin, std::move(callback)));
}

void InspectorCacheStorageAgent::requestEntries(
    const String& cache_id,
    int skip_count,
    int page_size,
    std::unique_ptr<RequestEntriesCallback> callback) {
  String cache_name;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  ProtocolResponse response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, &cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  DataRequestParams params;
  params.cache_name = cache_name;
  params.page_size = page_size;
  params.skip_count = skip_count;
  cache->DispatchOpen(
      WTF::MakeUnique<GetCacheForRequestData>(params, std::move(callback)),
      WebString(cache_name));
}

void InspectorCacheStorageAgent::deleteCache(
    const String& cache_id,
    std::unique_ptr<DeleteCacheCallback> callback) {
  String cache_name;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  ProtocolResponse response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, &cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->DispatchDelete(WTF::MakeUnique<DeleteCache>(std::move(callback)),
                        WebString(cache_name));
}

void InspectorCacheStorageAgent::deleteEntry(
    const String& cache_id,
    const String& request,
    std::unique_ptr<DeleteEntryCallback> callback) {
  String cache_name;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  ProtocolResponse response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, &cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->DispatchOpen(WTF::MakeUnique<GetCacheForDeleteEntry>(
                          request, cache_name, std::move(callback)),
                      WebString(cache_name));
}

void InspectorCacheStorageAgent::requestCachedResponse(
    const String& cache_id,
    const String& request_url,
    std::unique_ptr<RequestCachedResponseCallback> callback) {
  String cache_name;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  ProtocolResponse response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, &cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  WebServiceWorkerRequest request;
  request.SetURL(KURL(kParsedURLString, request_url));
  ExecutionContext* context = nullptr;
  response = GetExecutionContext(frames_, cache_id, &context);
  if (!response.isSuccess())
    return callback->sendFailure(response);

  cache->DispatchMatch(WTF::MakeUnique<CachedResponseMatchCallback>(
                           context, std::move(callback)),
                       request, WebServiceWorkerCache::QueryParams());
}
}  // namespace blink
