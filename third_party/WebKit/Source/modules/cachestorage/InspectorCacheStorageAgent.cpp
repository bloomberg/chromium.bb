// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/InspectorCacheStorageAgent.h"

#include <algorithm>
#include <memory>
#include <utility>
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"
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
using blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::DataEntry;
using blink::protocol::Response;

typedef blink::protocol::CacheStorage::Backend::DeleteCacheCallback
    DeleteCacheCallback;
typedef blink::protocol::CacheStorage::Backend::DeleteEntryCallback
    DeleteEntryCallback;
typedef blink::protocol::CacheStorage::Backend::RequestCacheNamesCallback
    RequestCacheNamesCallback;
typedef blink::protocol::CacheStorage::Backend::RequestEntriesCallback
    RequestEntriesCallback;
typedef blink::WebServiceWorkerCache::BatchOperation BatchOperation;

namespace blink {

namespace {

String BuildCacheId(const String& security_origin, const String& cache_name) {
  String id(security_origin);
  id.append('|');
  id.append(cache_name);
  return id;
}

Response ParseCacheId(const String& id,
                      String* security_origin,
                      String* cache_name) {
  size_t pipe = id.find('|');
  if (pipe == WTF::kNotFound)
    return Response::Error("Invalid cache id.");
  *security_origin = id.Substring(0, pipe);
  *cache_name = id.Substring(pipe + 1);
  return Response::OK();
}

Response AssertCacheStorage(
    const String& security_origin,
    std::unique_ptr<WebServiceWorkerCacheStorage>& result) {
  RefPtr<SecurityOrigin> sec_origin =
      SecurityOrigin::CreateFromString(security_origin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!sec_origin->IsPotentiallyTrustworthy())
    return Response::Error(sec_origin->IsPotentiallyTrustworthyErrorMessage());

  std::unique_ptr<WebServiceWorkerCacheStorage> cache =
      Platform::Current()->CreateCacheStorage(WebSecurityOrigin(sec_origin));
  if (!cache)
    return Response::Error("Could not find cache storage.");
  result = std::move(cache);
  return Response::OK();
}

Response AssertCacheStorageAndNameForId(
    const String& cache_id,
    String* cache_name,
    std::unique_ptr<WebServiceWorkerCacheStorage>& result) {
  String security_origin;
  Response response = ParseCacheId(cache_id, &security_origin, cache_name);
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

class RequestCacheNames
    : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
  WTF_MAKE_NONCOPYABLE(RequestCacheNames);

 public:
  RequestCacheNames(const String& security_origin,
                    std::unique_ptr<RequestCacheNamesCallback> callback)
      : security_origin_(security_origin), callback_(std::move(callback)) {}

  ~RequestCacheNames() override {}

  void OnSuccess(const WebVector<WebString>& caches) override {
    std::unique_ptr<Array<Cache>> array = Array<Cache>::create();
    for (size_t i = 0; i < caches.size(); i++) {
      String name = String(caches[i]);
      std::unique_ptr<Cache> entry =
          Cache::create()
              .setSecurityOrigin(security_origin_)
              .setCacheName(name)
              .setCacheId(BuildCacheId(security_origin_, name))
              .build();
      array->addItem(std::move(entry));
    }
    callback_->sendSuccess(std::move(array));
  }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(Response::Error(
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
  RequestResponse() {}
  RequestResponse(const String& request, const String& response)
      : request(request), response(response) {}
  String request;
  String response;
  double response_time;
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
    request_response.request = request.Url().GetString();
    request_response.response = response.StatusText();
    request_response.response_time = response.ResponseTime().ToDoubleT();

    if (--num_responses_left_ != 0)
      return;

    std::sort(responses_.begin(), responses_.end(),
              [](const RequestResponse& a, const RequestResponse& b) {
                return WTF::CodePointCompareLessThan(a.request, b.request);
              });
    if (params_.skip_count > 0)
      responses_.erase(0, params_.skip_count);
    bool has_more = false;
    if (static_cast<size_t>(params_.page_size) < responses_.size()) {
      responses_.erase(params_.page_size,
                       responses_.size() - params_.page_size);
      has_more = true;
    }
    std::unique_ptr<Array<DataEntry>> array = Array<DataEntry>::create();
    for (const auto& request_response : responses_) {
      std::unique_ptr<DataEntry> entry =
          DataEntry::create()
              .setRequest(request_response.request)
              .setResponse(request_response.response)
              .setResponseTime(request_response.response_time)
              .build();
      array->addItem(std::move(entry));
    }
    callback_->sendSuccess(std::move(array), has_more);
  }

  void SendFailure(const Response& error) { callback_->sendFailure(error); }

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
                                  PassRefPtr<ResponsesAccumulator> accum)
      : params_(params), request_(request), accumulator_(std::move(accum)) {}
  ~GetCacheResponsesForRequestData() override {}

  void OnSuccess(const WebServiceWorkerResponse& response) override {
    accumulator_->AddRequestResponsePair(request_, response);
  }

  void OnError(WebServiceWorkerCacheError error) override {
    accumulator_->SendFailure(Response::Error(
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
        AdoptRef(new ResponsesAccumulator(requests.size(), params_,
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
    callback_->sendFailure(Response::Error(
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
    callback_->sendFailure(Response::Error(String::Format(
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
  DeleteCache(std::unique_ptr<DeleteCacheCallback> callback)
      : callback_(std::move(callback)) {}
  ~DeleteCache() override {}

  void OnSuccess() override { callback_->sendSuccess(); }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(Response::Error(
        String::Format("Error requesting cache names: %s",
                       ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  std::unique_ptr<DeleteCacheCallback> callback_;
};

class DeleteCacheEntry : public WebServiceWorkerCache::CacheBatchCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCacheEntry);

 public:
  DeleteCacheEntry(std::unique_ptr<DeleteEntryCallback> callback)
      : callback_(std::move(callback)) {}
  ~DeleteCacheEntry() override {}

  void OnSuccess() override { callback_->sendSuccess(); }

  void OnError(WebServiceWorkerCacheError error) override {
    callback_->sendFailure(Response::Error(
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
    callback_->sendFailure(Response::Error(String::Format(
        "Error requesting cache %s: %s", cache_name_.Utf8().data(),
        ServiceWorkerCacheErrorString(error).data())));
  }

 private:
  String request_spec_;
  String cache_name_;
  std::unique_ptr<DeleteEntryCallback> callback_;
};

}  // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent() = default;

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() = default;

DEFINE_TRACE(InspectorCacheStorageAgent) {
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
    callback->sendSuccess(Array<protocol::CacheStorage::Cache>::create());
    return;
  }

  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  Response response = AssertCacheStorage(security_origin, cache);
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
  Response response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, cache);
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
  Response response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, cache);
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
  Response response =
      AssertCacheStorageAndNameForId(cache_id, &cache_name, cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->DispatchOpen(WTF::MakeUnique<GetCacheForDeleteEntry>(
                          request, cache_name, std::move(callback)),
                      WebString(cache_name));
}

}  // namespace blink
